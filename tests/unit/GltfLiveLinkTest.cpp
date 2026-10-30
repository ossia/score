// glTF Live Link: the wire protocol, the node's override bookkeeping, and the
// whole path from a TCP client to a published scene.
//
// Contract under test, from Threedim/LiveLink/Protocol.hpp and
// Threedim/GltfLiveLink.cpp:
//
//  - frames are u32 length | u8 type | body; a zero or oversized length
//    poisons the decoder (the stream cannot be resynchronised), a malformed
//    body is rejected whole;
//  - a transforms record overrides the first scene_transform of every node of
//    that name; untouched subtrees are shared by pointer with the parsed scene;
//  - seqs order scenes and transforms: a transform older than the current
//    scene is ignored, a new scene drops the overrides it already contains,
//    and a scene older than the current one is ignored;
//  - a .glb sent over TCP comes out of operator()() parsed, with the streamed
//    transforms applied on top.

#include <Threedim/GltfLiveLink.hpp>
#include <Threedim/LiveLink/Protocol.hpp>

#include <Gfx/Graph/SceneGPUState.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <ossia/dataflow/geometry_port.hpp>

#include <boost/asio/connect.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace Threedim;
using namespace Threedim::LiveLink;

namespace
{
template <typename T>
void put(std::string& out, T v)
{
  char buf[sizeof(T)];
  std::memcpy(buf, &v, sizeof(T));
  out.append(buf, sizeof(T));
}

// The minimal one-triangle GLB of GltfLoaderTest.cpp, whose single node is
// named "tri".
// `material`: JSON appended to the glTF's top level and the primitive's
// material index, e.g. R"("materials":[...],)" and 0.
std::string triangle_glb(std::string_view material = {}, int material_index = -1)
{
  std::string bin;
  const float pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  const uint16_t idx[3] = {0, 1, 2};
  for(float f : pos)
    put(bin, f);
  for(uint16_t i : idx)
    put(bin, i);

  // clang-format off
  std::string json = R"({"asset":{"version":"2.0"},)"
    R"("scene":0,"scenes":[{"nodes":[0]}],)"
    R"("nodes":[{"mesh":0,"name":"tri"}],)";
  json += material;
  json += material_index < 0
      ? std::string{R"("meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],)"}
      : R"("meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":)"
            + std::to_string(material_index) + "}]}],";
  json +=
    R"("buffers":[{"byteLength":42}],)"
    R"("bufferViews":[)"
      R"({"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},)"
      R"({"buffer":0,"byteOffset":36,"byteLength":6,"target":34963}],)"
    R"("accessors":[)"
      R"({"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,)"
        R"("type":"VEC3","min":[0,0,0],"max":[1,1,0]},)"
      R"({"bufferView":1,"byteOffset":0,"componentType":5123,"count":3,)"
        R"("type":"SCALAR"}]})";
  // clang-format on

  while(json.size() % 4)
    json.push_back(' ');
  while(bin.size() % 4)
    bin.push_back('\0');

  std::string out;
  put<uint32_t>(out, 0x46546C67); // 'glTF'
  put<uint32_t>(out, 2);
  put<uint32_t>(out, uint32_t(12 + 8 + json.size() + 8 + bin.size()));
  put<uint32_t>(out, uint32_t(json.size()));
  put<uint32_t>(out, 0x4E4F534A); // 'JSON'
  out += json;
  put<uint32_t>(out, uint32_t(bin.size()));
  put<uint32_t>(out, 0x004E4942); // 'BIN\0'
  out += bin;
  return out;
}

transform_record record(std::string name, float tx, float ty, float tz)
{
  transform_record r;
  r.name = std::move(name);
  r.translation[0] = tx;
  r.translation[1] = ty;
  r.translation[2] = tz;
  return r;
}

ossia::scene_node_ptr
make_node(uint64_t id, std::string name, float tx, std::vector<ossia::scene_payload> rest = {})
{
  ossia::scene_transform xf{};
  xf.translation[0] = tx;
  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(xf);
  for(auto& p : rest)
    children->push_back(std::move(p));

  auto n = std::make_shared<ossia::scene_node>();
  n->id.value = id;
  n->name = std::move(name);
  n->children = std::move(children);
  return n;
}

// A: id 1, x 10, with child B: id 2, x 20. C: id 3, x 30, a separate root.
std::shared_ptr<ossia::scene_state> make_scene(float b_x = 20.f)
{
  auto b = make_node(2, "B", b_x);
  auto a = make_node(1, "A", 10.f, {ossia::scene_payload{b}});
  auto c = make_node(3, "C", 30.f);

  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(a);
  roots->push_back(c);
  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::move(roots);
  return st;
}

const ossia::scene_node* find(const ossia::scene_node_ptr& n, std::string_view name)
{
  if(!n)
    return nullptr;
  if(n->name == name)
    return n.get();
  if(n->children)
    for(const auto& c : *n->children)
      if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&c))
        if(auto* hit = find(*sub, name))
          return hit;
  return nullptr;
}

const ossia::scene_node* find(const ossia::scene_state& st, std::string_view name)
{
  if(st.roots)
    for(const auto& r : *st.roots)
      if(auto* hit = find(r, name))
        return hit;
  return nullptr;
}

float x_of(const GltfLiveLink& node, std::string_view name)
{
  const auto& st = node.outputs.scene_out.scene.state;
  REQUIRE(st);
  const auto* n = find(*st, name);
  REQUIRE(n);
  REQUIRE(n->children);
  const auto* xf = ossia::get_if<ossia::scene_transform>(&n->children->front());
  REQUIRE(xf);
  return xf->translation[0];
}

void quiet(GltfLiveLink& node)
{
  // No socket for the tests that drive the node directly.
  node.inputs.listen.value = false;
}
}

TEST_CASE("live link frames round-trip through the decoder", "[livelink]")
{
  const std::vector<transform_record> recs{
      record("Cube", 1, 2, 3), record("Caméra", -1, 0, 0.5f)};
  const std::string stream = encode_frame(frame_type::hello, R"({"protocol":1})")
                             + encode_scene(7, triangle_glb())
                             + encode_transforms(8, recs);

  // Byte by byte: frames must come out whole whatever the read boundaries.
  frame_decoder dec;
  std::vector<std::pair<frame_type, std::string>> frames;
  for(char c : stream)
    REQUIRE(dec.feed(std::string_view{&c, 1}, [&](frame_type t, std::string_view b) {
      frames.emplace_back(t, std::string{b});
    }));
  REQUIRE(dec.buffered() == 0);
  REQUIRE(frames.size() == 3);
  CHECK(frames[0].first == frame_type::hello);

  REQUIRE(frames[1].first == frame_type::scene);
  auto scene = parse_scene(frames[1].second);
  REQUIRE(scene);
  CHECK(scene->seq == 7);
  CHECK(scene->glb == triangle_glb());

  REQUIRE(frames[2].first == frame_type::transforms);
  auto xf = parse_transforms(frames[2].second);
  REQUIRE(xf);
  CHECK(xf->seq == 8);
  REQUIRE(xf->records.size() == 2);
  CHECK(xf->records[1].name == "Caméra");
  CHECK(xf->records[1].translation[2] == 0.5f);
  CHECK(xf->records[0].rotation[3] == 1.f);
  CHECK(xf->records[0].scale[1] == 1.f);
}

TEST_CASE("live link rejects malformed input", "[livelink]")
{
  auto noop = [](frame_type, std::string_view) {};

  SECTION("a zero length poisons the stream")
  {
    frame_decoder dec;
    std::string zero(4, '\0');
    CHECK_FALSE(dec.feed(zero, noop));
    CHECK(dec.failed());
    CHECK_FALSE(dec.feed(encode_frame(frame_type::hello, "{}"), noop));
  }

  SECTION("an oversized length poisons the stream without allocating it")
  {
    frame_decoder dec;
    std::string big;
    put<uint32_t>(big, max_frame_size + 1);
    CHECK_FALSE(dec.feed(big, noop));
    CHECK(dec.buffered() == 0);
  }

  SECTION("a scene body must hold a .glb")
  {
    std::string body;
    put<uint64_t>(body, 1);
    body += "not a binary gltf at all";
    CHECK_FALSE(parse_scene(body));
    CHECK_FALSE(parse_scene("short"));
  }

  SECTION("a transforms body is all or nothing")
  {
    const auto frame = encode_transforms(3, {record("A", 1, 2, 3)});
    const std::string_view body{frame.data() + 5, frame.size() - 5};
    REQUIRE(parse_transforms(body));
    for(std::size_t n = 0; n < body.size(); ++n)
      CHECK_FALSE(parse_transforms(body.substr(0, n)));
    CHECK_FALSE(parse_transforms(std::string{body} + "x"));

    // A count far beyond what the body can hold is refused before any
    // allocation sized from it.
    std::string lie;
    put<uint64_t>(lie, 1);
    put<uint32_t>(lie, 0xFFFFFFFFu);
    CHECK_FALSE(parse_transforms(lie));
  }
}

TEST_CASE("live link overrides streamed node transforms", "[livelink]")
{
  GltfLiveLink node;
  quiet(node);

  node();
  CHECK_FALSE(node.outputs.scene_out.scene.state);

  auto parsed = make_scene();
  node.set_scene(parsed, 5);
  node();
  CHECK(x_of(node, "A") == 10.f);
  CHECK(x_of(node, "B") == 20.f);

  SECTION("a newer transform overrides, and only its branch is cloned")
  {
    node.set_transform(record("B", 2, 0, 0), 6);
    node();
    CHECK(x_of(node, "B") == 2.f);
    CHECK(x_of(node, "A") == 10.f);

    // C holds no overridden node: it is the parsed C, by pointer.
    const auto* c_out = find(*node.outputs.scene_out.scene.state, "C");
    CHECK(c_out == (*parsed->roots)[1].get());
    CHECK(node.outputs.scene_out.dirty == ossia::scene_port::dirty_animation);
  }

  SECTION("a transform older than the scene is ignored")
  {
    node.set_transform(record("B", 2, 0, 0), 4);
    node();
    CHECK(x_of(node, "B") == 20.f);
  }

  SECTION("an unknown name changes nothing")
  {
    node.set_transform(record("Nope", 2, 0, 0), 6);
    node();
    CHECK(x_of(node, "A") == 10.f);
    CHECK(x_of(node, "B") == 20.f);
  }

  SECTION("a new scene drops the overrides it contains, keeps newer ones")
  {
    node.set_transform(record("A", 1, 0, 0), 6);
    node.set_transform(record("B", 2, 0, 0), 8);
    node.set_scene(make_scene(21.f), 7);
    node();
    CHECK(x_of(node, "A") == 10.f); // seq 6 <= 7: the scene has it
    CHECK(x_of(node, "B") == 2.f);  // seq 8 >  7: still ahead of the scene
    CHECK(node.outputs.scene_out.dirty == ossia::scene_port::dirty_all);
  }

  SECTION("a scene older than the current one is ignored")
  {
    node.set_scene(make_scene(99.f), 3);
    node();
    CHECK(x_of(node, "B") == 20.f);
  }

  SECTION("the newest transform of a node wins")
  {
    node.set_transform(record("B", 3, 0, 0), 9);
    node.set_transform(record("B", 2, 0, 0), 8);
    node();
    CHECK(x_of(node, "B") == 3.f);
  }
}

TEST_CASE("live link publishes a .glb received over TCP", "[livelink]")
{
  namespace asio = boost::asio;
  using tcp = asio::ip::tcp;
  asio::io_context ctx;

  // A port nobody uses right now.
  uint16_t port{};
  {
    tcp::acceptor probe{ctx, tcp::endpoint{asio::ip::make_address("127.0.0.1"), 0}};
    port = probe.local_endpoint().port();
  }

  GltfLiveLink node;
  node.inputs.port.value = port;
  node.inputs.bind.value = "127.0.0.1";
  node.inputs.listen.value = true;
  node(); // starts listening

  tcp::socket sock{ctx};
  sock.connect(tcp::endpoint{asio::ip::make_address("127.0.0.1"), port});
  const std::string frames
      = encode_frame(frame_type::hello, R"({"protocol":1,"app":"test"})")
        + encode_scene(1, triangle_glb())
        + encode_transforms(2, {record("tri", 4, 5, 6)});
  asio::write(sock, asio::buffer(frames));

  // No worker is bound in a test, so the node parses on its own thread as
  // soon as the scene is in; the network thread may need a moment.
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  const ossia::scene_node* tri = nullptr;
  while(std::chrono::steady_clock::now() < deadline)
  {
    node();
    if(const auto& st = node.outputs.scene_out.scene.state)
    {
      tri = find(*st, "tri");
      if(tri)
      {
        const auto* xf = ossia::get_if<ossia::scene_transform>(&tri->children->front());
        if(xf && xf->translation[0] == 4.f)
          break;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  REQUIRE(tri);
  const auto* xf = ossia::get_if<ossia::scene_transform>(&tri->children->front());
  REQUIRE(xf);
  CHECK(xf->translation[0] == 4.f);
  CHECK(xf->translation[1] == 5.f);
  CHECK(xf->translation[2] == 6.f);

  // The parsed triangle is under the override: the mesh made it through.
  bool has_mesh = false;
  for(const auto& c : *tri->children)
    has_mesh = has_mesh || ossia::get_if<ossia::mesh_component_ptr>(&c);
  CHECK(has_mesh);

  // Turning Listen off closes the port.
  node.inputs.listen.value = false;
  node();
  tcp::socket again{ctx};
  boost::system::error_code ec;
  again.connect(tcp::endpoint{asio::ip::make_address("127.0.0.1"), port}, ec);
  CHECK(ec);
}


namespace
{
std::shared_ptr<ossia::buffer_resource> float_buffer(std::vector<float> v)
{
  auto data = std::make_shared<std::vector<float>>(std::move(v));
  auto br = std::make_shared<ossia::buffer_resource>();
  ossia::buffer_data bd;
  bd.data = std::shared_ptr<const void>(data, data->data());
  bd.byte_size = int64_t(data->size() * sizeof(float));
  br->resource = std::move(bd);
  return br;
}

const float* floats_of(const ossia::buffer_resource_ptr& b)
{
  const auto* bd = ossia::get_if<ossia::buffer_data>(&b->resource);
  REQUIRE(bd);
  return static_cast<const float*>(bd->data.get());
}

template <typename T>
const T* component(const ossia::scene_node& n)
{
  for(const auto& c : *n.children)
    if(auto* p = ossia::get_if<std::shared_ptr<const T>>(&c))
      return p->get();
  return nullptr;
}

// Rig (id 1) > Bone (id 2); Lamp (3) with a point light; Cam (4) with a
// camera; Blob (5) with a one-triangle mesh using material "Mat", one morph
// target moving vertex 1 by +2 in x. The skeleton's single joint is Bone.
struct rich_scene
{
  std::shared_ptr<ossia::scene_state> state;
  std::shared_ptr<ossia::material_component> mat;
};

rich_scene make_rich_scene()
{
  rich_scene out;
  out.mat = std::make_shared<ossia::material_component>();
  out.mat->roughness_factor = 0.5f;

  auto light = std::make_shared<ossia::light_component>();
  light->intensity = 1.f;
  auto cam = std::make_shared<ossia::camera_component>();

  auto mesh = std::make_shared<ossia::mesh_component>();
  ossia::mesh_primitive prim;
  prim.vertex_count = 3;
  prim.vertex_buffers.push_back(float_buffer({0, 0, 0, 1, 0, 0, 0, 1, 0}));
  ossia::vertex_attribute pos;
  pos.semantic = ossia::attribute_semantic::position;
  pos.format = ossia::vertex_format::float3;
  pos.buffer_index = 0;
  pos.byte_stride = 12;
  prim.attributes.push_back(pos);
  prim.material = out.mat;
  prim.stable_id = 42;
  mesh->primitives.push_back(std::move(prim));
  ossia::morph_target t;
  t.position_deltas = float_buffer({0, 0, 0, 2, 0, 0, 0, 0, 0});
  mesh->morph_targets.push_back(t);
  mesh->morph_weights.push_back(0.f);

  auto bone = make_node(2, "Bone", 0.f);
  auto rig = make_node(1, "Rig", 0.f, {ossia::scene_payload{bone}});
  auto lamp = make_node(3, "Lamp", 0.f, {ossia::scene_payload{ossia::light_component_ptr(light)}});
  auto camn = make_node(4, "Cam", 0.f, {ossia::scene_payload{ossia::camera_component_ptr(cam)}});
  auto blob = make_node(5, "Blob", 0.f, {ossia::scene_payload{ossia::mesh_component_ptr(mesh)}});

  auto skel = std::make_shared<ossia::skeleton_component>();
  skel->joints.resize(1);
  skel->joint_node_ids.push_back(ossia::scene_node_id{2});

  out.state = std::make_shared<ossia::scene_state>();
  out.state->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>(
      std::vector<ossia::scene_node_ptr>{rig, lamp, camn, blob});
  out.state->materials = std::make_shared<std::vector<ossia::material_component_ptr>>(
      std::vector<ossia::material_component_ptr>{out.mat});
  out.state->skeletons = std::make_shared<std::vector<ossia::skeleton_component_ptr>>(
      std::vector<ossia::skeleton_component_ptr>{skel});
  return out;
}

uint64_t scene_prim_id(const rich_scene& s)
{
  const auto& blob = (*s.state->roots)[3];
  for(const auto& c : *blob->children)
    if(auto* m = ossia::get_if<ossia::mesh_component_ptr>(&c))
      return (*m)->primitives[0].stable_id;
  return ~0ull;
}

template <typename R>
void put_record(LiveLink::latest<R>& map, std::string key, uint64_t seq, R r)
{
  map[std::move(key)] = LiveLink::stamped<R>{seq, std::move(r)};
}
}

TEST_CASE("live link record frames round-trip", "[livelink]")
{
  armature_pose pose;
  pose.armature = "Rig";
  pose.bones.push_back(record("Spine", 1, 2, 3));
  pose.bones.push_back(record("Neck", 4, 5, 6));
  auto joints = parse_records<armature_pose>(
      encode_records(frame_type::joints, 3, std::vector{pose}).substr(5));
  REQUIRE(joints);
  REQUIRE(joints->records.size() == 1);
  CHECK(joints->records[0].armature == "Rig");
  REQUIRE(joints->records[0].bones.size() == 2);
  CHECK(joints->records[0].bones[1].name == "Neck");
  CHECK(joints->records[0].bones[1].translation[2] == 6.f);

  weights_record w{"Face", {0.25f, 0.75f}};
  auto weights = parse_records<weights_record>(
      encode_records(frame_type::weights, 4, std::vector{w}).substr(5));
  REQUIRE(weights);
  CHECK(weights->records[0].weights == std::vector<float>{0.25f, 0.75f});

  light_record l;
  l.name = "Lamp";
  l.type = light_record::spot;
  l.intensity = 12.f;
  l.outer_cone = 0.5f;
  auto lights = parse_records<light_record>(
      encode_records(frame_type::lights, 5, std::vector{l}).substr(5));
  REQUIRE(lights);
  CHECK(lights->records[0].type == light_record::spot);
  CHECK(lights->records[0].outer_cone == 0.5f);

  camera_record c;
  c.name = "Cam";
  c.yfov = 0.9f;
  auto cams = parse_records<camera_record>(
      encode_records(frame_type::cameras, 6, std::vector{c}).substr(5));
  REQUIRE(cams);
  CHECK(cams->records[0].yfov == 0.9f);

  material_record m;
  m.name = "Mat";
  m.fields = material_record::roughness_field;
  m.roughness = 0.125f;
  auto mats = parse_records<material_record>(
      encode_records(frame_type::materials, 7, std::vector{m}).substr(5));
  REQUIRE(mats);
  CHECK(mats->records[0].fields == material_record::roughness_field);
  CHECK(mats->records[0].roughness == 0.125f);

  visibility_record v{"Blob", 0};
  auto vis = parse_records<visibility_record>(
      encode_records(frame_type::visibility, 8, std::vector{v}).substr(5));
  REQUIRE(vis);
  CHECK(vis->records[0].visible == 0);

  // Every type is all-or-nothing.
  const auto frame = encode_records(frame_type::lights, 5, std::vector{l});
  const std::string_view body{frame.data() + 5, frame.size() - 5};
  for(std::size_t n = 0; n < body.size(); ++n)
    CHECK_FALSE(parse_records<light_record>(body.substr(0, n)));

  // A bone count far beyond the body is refused before allocating.
  std::string lie;
  put<uint64_t>(lie, 1);
  put<uint32_t>(lie, 1);
  put<uint16_t>(lie, 3);
  lie += "Rig";
  put<uint32_t>(lie, 0x7FFFFFFFu);
  CHECK_FALSE(parse_records<armature_pose>(lie));
}

TEST_CASE("live link overrides lights, cameras, materials, shape keys, bones, visibility",
          "[livelink]")
{
  GltfLiveLink node;
  quiet(node);
  auto scene = make_rich_scene();
  node.set_scene(scene.state, 1, {"Mat"});
  node();

  LiveLink::inbox box;
  {
    light_record l;
    l.name = "Lamp";
    l.type = light_record::spot;
    l.color[1] = 0.25f;
    l.intensity = 40.f;
    l.inner_cone = 0.2f;
    l.outer_cone = 0.6f;
    put_record(box.lights, "Lamp", 2, l);

    camera_record c;
    c.name = "Cam";
    c.yfov = 0.4f;
    c.aspect = 2.f;
    c.znear = 0.5f;
    c.zfar = 50.f;
    put_record(box.cameras, "Cam", 2, c);

    material_record m;
    m.name = "Mat";
    m.fields = material_record::base_color_rgb | material_record::emissive;
    m.base_color[0] = 0.1f;
    m.base_color[3] = 0.3f; // not in fields: ignored
    m.emissive_factor[2] = 1.f;
    m.emissive_strength = 4.f;
    put_record(box.materials, "Mat", 2, m);

    put_record(box.weights, "Blob", 2, weights_record{"Blob", {0.5f}});
    put_record(box.visibility, "Cam", 2, visibility_record{"Cam", 0});
    put_record(
        box.joints, joint_key("Rig", "Bone"), 2,
        joint_record{"Rig", record("Bone", 7, 0, 0)});
  }
  node.apply(std::move(box));
  node();

  const auto& st = *node.outputs.scene_out.scene.state;

  const auto* lamp = component<ossia::light_component>(*find(st, "Lamp"));
  REQUIRE(lamp);
  CHECK(lamp->type == ossia::light_type::spot);
  CHECK(lamp->color[1] == 0.25f);
  CHECK(lamp->intensity == 40.f);
  CHECK(lamp->outer_cone_angle == 0.6f);

  const auto* camn = find(st, "Cam");
  CHECK_FALSE(camn->visible);
  const auto* cam = component<ossia::camera_component>(*camn);
  REQUIRE(cam);
  CHECK(cam->yfov == 0.4f);
  CHECK(cam->aspect_ratio == 2.f);
  CHECK(cam->zfar == 50.f);

  // The material is replaced in the list and in the primitive using it.
  REQUIRE(st.materials);
  const auto& mat = (*st.materials)[0];
  CHECK(mat.get() != scene.mat.get());
  CHECK(mat->base_color_factor[0] == 0.1f);
  CHECK(mat->base_color_factor[3] == 1.f);
  CHECK(mat->roughness_factor == 0.5f); // untouched field kept
  CHECK(mat->emissive_factor[2] == 1.f);
  CHECK(mat->emissive_strength == 4.f);
  const auto* blob = component<ossia::mesh_component>(*find(st, "Blob"));
  REQUIRE(blob);
  CHECK(blob->primitives[0].material == mat);

  // Shape key at 0.5: vertex 1 moved half its +2 delta; the bounds follow.
  const float* p = floats_of(blob->primitives[0].vertex_buffers[0]);
  CHECK(p[3] == 2.f);
  CHECK(p[0] == 0.f);
  CHECK(blob->primitives[0].bounds.max[0] == 2.f);
  CHECK(blob->morph_weights[0] == 0.5f);
  // A new id, or the renderer would keep drawing the vertices it uploaded
  // under the old one.
  const auto blended_id = blob->primitives[0].stable_id;
  CHECK(blended_id != scene_prim_id(scene));

  // The bone: its node, and the joint the renderer skins with.
  CHECK(x_of(node, "Bone") == 7.f);
  REQUIRE(st.skeletons);
  CHECK((*st.skeletons)[0]->joints[0].translation[0] == 7.f);

  SECTION("weights re-blend from the parsed mesh, without drift")
  {
    for(float w : {0.9f, 0.1f, 0.75f, 0.f})
    {
      LiveLink::inbox b;
      static uint64_t seq = 10;
      put_record(b.weights, "Blob", ++seq, weights_record{"Blob", {w}});
      node.apply(std::move(b));
      node();
    }
    const auto* m = component<ossia::mesh_component>(
        *find(*node.outputs.scene_out.scene.state, "Blob"));
    CHECK(floats_of(m->primitives[0].vertex_buffers[0])[3] == 1.f);
    // Back at the parsed weights: the parsed primitive, id included.
    CHECK(m->primitives[0].stable_id == scene_prim_id(scene));
  }

  SECTION("the same weights give the same id back")
  {
    for(float w : {0.9f, 0.5f})
    {
      LiveLink::inbox b;
      static uint64_t seq = 100;
      put_record(b.weights, "Blob", ++seq, weights_record{"Blob", {w}});
      node.apply(std::move(b));
      node();
    }
    const auto* m = component<ossia::mesh_component>(
        *find(*node.outputs.scene_out.scene.state, "Blob"));
    CHECK(m->primitives[0].stable_id == blended_id);
  }

  SECTION("a change leaves what the other overrides derived as it was")
  {
    // The renderer keys material and light slots and mesh uploads by
    // pointer: a component that is handed back anew is processed anew.
    auto pointers = [&] {
      const auto& s = *node.outputs.scene_out.scene.state;
      return std::tuple{
          component<ossia::light_component>(*find(s, "Lamp")),
          component<ossia::camera_component>(*find(s, "Cam")),
          (*s.materials)[0].get(),
          component<ossia::mesh_component>(*find(s, "Blob"))};
    };
    auto send = [&](auto fill) {
      static uint64_t seq = 1000;
      LiveLink::inbox b;
      fill(b, ++seq);
      node.apply(std::move(b));
      node();
    };
    const auto [lamp0, cam0, mat0, mesh0] = pointers();

    // Moving the blob, hiding the lamp.
    send([](auto& b, uint64_t seq) {
      put_record(b.transforms, "Blob", seq, record("Blob", 3, 0, 0));
      put_record(b.visibility, "Lamp", seq, visibility_record{"Lamp", 0});
    });
    CHECK(x_of(node, "Blob") == 3.f);
    CHECK(pointers() == std::tuple{lamp0, cam0, mat0, mesh0});

    // Another material value: a new material, and with it a new mesh using
    // it, but the same blend (and so the same vertex buffer).
    send([](auto& b, uint64_t seq) {
      material_record m;
      m.name = "Mat";
      m.fields = material_record::roughness_field;
      m.roughness = 0.25f;
      put_record(b.materials, "Mat", seq, m);
    });
    const auto [lamp1, cam1, mat1, mesh1] = pointers();
    CHECK(lamp1 == lamp0);
    CHECK(cam1 == cam0);
    CHECK(mat1 != mat0);
    CHECK(mat1->roughness_factor == 0.25f);
    REQUIRE(mesh1 != mesh0);
    CHECK(mesh1->primitives[0].material.get() == mat1);
    CHECK(mesh1->primitives[0].stable_id == blended_id);

    // A light value: only the light is new.
    send([](auto& b, uint64_t seq) {
      light_record l;
      l.name = "Lamp";
      l.intensity = 2.f;
      put_record(b.lights, "Lamp", seq, l);
    });
    const auto [lamp2, cam2, mat2, mesh2] = pointers();
    CHECK(lamp2 != lamp1);
    CHECK(lamp2->intensity == 2.f);
    CHECK(std::tuple{cam2, mat2, mesh2} == std::tuple{cam1, mat1, mesh1});
  }

  SECTION("a new scene drops the overrides it contains")
  {
    node.set_scene(make_rich_scene().state, 3, {"Mat"});
    node();
    const auto& st2 = *node.outputs.scene_out.scene.state;
    CHECK(component<ossia::light_component>(*find(st2, "Lamp"))->intensity == 1.f);
    CHECK(find(st2, "Cam")->visible);
    CHECK((*st2.materials)[0]->base_color_factor[0] == 1.f);
  }
}

TEST_CASE("live link adds patches and removes nodes, in seq order", "[livelink]")
{
  GltfLiveLink node;
  quiet(node);
  node.set_scene(make_scene(), 1);
  node();

  auto patch = [](uint64_t seq) {
    return LiveLink::structural_message{
        .seq = seq, .type = frame_type::patch, .glb = triangle_glb()};
  };
  auto removal = [](uint64_t seq, std::vector<std::string> names) {
    return LiveLink::structural_message{
        .seq = seq, .type = frame_type::remove, .names = std::move(names)};
  };
  auto send = [&](auto... msgs) {
    LiveLink::inbox b;
    (b.structural.push_back(std::move(msgs)), ...);
    node.apply(std::move(b));
    node();
  };
  auto state = [&]() -> const ossia::scene_state& {
    return *node.outputs.scene_out.scene.state;
  };

  // The patch's root joins the scene's, with an id past the scene's own.
  send(patch(2));
  const auto* tri = find(state(), "tri");
  REQUIRE(tri);
  CHECK(tri->id.value > 3);
  CHECK(find(state(), "A"));
  CHECK(find(state(), "C"));

  // It is a node like the others: values stream to it.
  {
    LiveLink::inbox b;
    put_record(b.transforms, "tri", 3, record("tri", 7, 0, 0));
    node.apply(std::move(b));
    node();
  }
  CHECK(x_of(node, "tri") == 7.f);

  // A removal takes the subtree: A, and B under it.
  send(removal(4, {"A"}));
  CHECK_FALSE(find(state(), "A"));
  CHECK_FALSE(find(state(), "B"));
  CHECK(find(state(), "C"));
  CHECK(find(state(), "tri"));

  SECTION("a removal waits for the patch before it to be parsed")
  {
    std::vector<GltfLiveLink::job> requests;
    node.worker.request = [&](GltfLiveLink::job j) { requests.push_back(std::move(j)); };
    send(patch(5));
    send(removal(6, {"tri", "C"}));
    REQUIRE(requests.size() == 1);
    // Not applied yet: it would have removed C ahead of the patch.
    CHECK(find(state(), "C"));

    REQUIRE(std::holds_alternative<GltfLiveLink::glb_job>(requests.front()));
    CHECK(std::get<GltfLiveLink::glb_job>(requests.front()).patch);
    auto done = GltfLiveLink::worker::work(std::move(requests.front()));
    REQUIRE(done);
    done(node);
    node();
    // Both triangles (the one of seq 2, and seq 5's) and C are gone.
    CHECK_FALSE(find(state(), "tri"));
    CHECK_FALSE(find(state(), "C"));
  }

  SECTION("a scene drops the changes it contains")
  {
    node.set_scene(make_scene(), 10);
    send(patch(9), removal(9, {"C"}));
    CHECK_FALSE(find(state(), "tri"));
    CHECK(find(state(), "A"));
    CHECK(find(state(), "C"));
  }
}

TEST_CASE("live link sets the environment from the world, across scenes", "[livelink]")
{
  GltfLiveLink node;
  quiet(node);
  node.set_scene(make_scene(), 1);
  node();

  LiveLink::inbox b;
  LiveLink::environment_record env{"World", {0.1f, 0.2f, 0.3f}, 2.f};
  b.environment["World"] = {2, env};
  node.apply(std::move(b));
  node();

  auto check = [&] {
    const auto& e = node.outputs.scene_out.scene.state->environment;
    CHECK(e.ambient_color[1] == 0.2f);
    CHECK(e.ambient_intensity == 2.f);
    CHECK((e.params_set & ossia::scene_environment::params_ambient));
  };
  check();

  // glTF has no world: a new scene keeps it.
  node.set_scene(make_scene(), 5);
  node();
  check();

  // The round trip of the new record types.
  auto envs = parse_records<LiveLink::environment_record>(
      encode_records(frame_type::environment, 3, std::vector{env}).substr(5));
  REQUIRE(envs);
  CHECK(envs->records[0].ambient[2] == 0.3f);
  auto rm = parse_records<LiveLink::node_name_record>(
      encode_records(frame_type::remove, 4, std::vector{LiveLink::node_name_record{"A"}})
          .substr(5));
  REQUIRE(rm);
  CHECK(rm->records[0].name == "A");
}

namespace
{
// The node holding the camera component, wherever it is.
const ossia::scene_node* camera_node(const ossia::scene_node_ptr& n)
{
  if(!n || !n->children)
    return nullptr;
  for(const auto& c : *n->children)
  {
    if(ossia::get_if<ossia::camera_component_ptr>(&c))
      return n.get();
    if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&c))
      if(auto* found = camera_node(*sub))
        return found;
  }
  return nullptr;
}

bool under_wrap(const ossia::scene_state& st, const ossia::scene_node* n)
{
  // The wrap is the first root; anything else is outside of it.
  std::function<bool(const ossia::scene_node_ptr&)> holds = [&](const ossia::scene_node_ptr& x) {
    if(x.get() == n)
      return true;
    if(x && x->children)
      for(const auto& c : *x->children)
        if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&c); sub && holds(*sub))
          return true;
    return false;
  };
  return holds(st.roots->front());
}
}

TEST_CASE("live link transform controls place the content, not the cameras", "[livelink]")
{
  GltfLiveLink node;
  quiet(node);
  auto scene = make_rich_scene();
  node.set_scene(scene.state, 1, {"Mat"});
  node.inputs.position.value = {5.f, 0.f, 0.f};
  node();

  const auto& st = *node.outputs.scene_out.scene.state;
  const auto* cam = camera_node(st.roots->back());
  REQUIRE(cam);
  CHECK_FALSE(under_wrap(st, cam));
  CHECK(cam->id.value == 4);
  CHECK(cam->name == "Cam");
  // The only "Cam" node is the lifted one.
  CHECK(find(st, "Cam") == cam);
  // The content is still under the wrap.
  CHECK(under_wrap(st, find(st, "Blob")));
  CHECK(under_wrap(st, find(st, "Lamp")));

  SECTION("Move cameras keeps them in the wrap")
  {
    node.inputs.move_cameras.value = true;
    node();
    const auto& st2 = *node.outputs.scene_out.scene.state;
    CHECK(under_wrap(st2, camera_node(st2.roots->front())));
  }
}

TEST_CASE("live link import toggles", "[livelink]")
{
  GltfLiveLink node;
  quiet(node);
  auto scene = make_rich_scene();
  node.set_scene(scene.state, 1, {"Mat"});
  node();

  LiveLink::inbox box;
  put_record(box.weights, "Blob", 2, weights_record{"Blob", {0.5f}});
  put_record(box.joints, joint_key("Rig", "Bone"), 2, joint_record{"Rig", record("Bone", 7, 0, 0)});
  node.apply(std::move(box));

  auto st = [&]() -> const ossia::scene_state& { return *node.outputs.scene_out.scene.state; };
  auto blob = [&] { return component<ossia::mesh_component>(*find(st(), "Blob")); };

  node.inputs.cameras.value = false;
  node.inputs.lights.value = false;
  node.inputs.materials.value = false;
  node.inputs.skinning.value = false;
  node.inputs.bone_poses.value = false;
  node.inputs.shape_keys.value = false;
  node();
  CHECK_FALSE(camera_node(st().roots->front()));
  CHECK_FALSE(component<ossia::light_component>(*find(st(), "Lamp")));
  CHECK_FALSE(blob()->primitives[0].material);
  CHECK((!st().materials || st().materials->empty()));
  CHECK((!st().skeletons || st().skeletons->empty()));
  CHECK(x_of(node, "Bone") == 0.f);          // the pose was not applied
  CHECK(blob()->morph_weights[0] == 0.f);    // nor the shape key
  const auto* stripped = blob();
  node();
  CHECK(blob() == stripped);                 // the same mesh, frame after frame

  node.inputs.cameras.value = true;
  node.inputs.lights.value = true;
  node.inputs.materials.value = true;
  node.inputs.skinning.value = true;
  node.inputs.bone_poses.value = true;
  node.inputs.shape_keys.value = true;
  node();
  CHECK(camera_node(st().roots->back()));
  CHECK(component<ossia::light_component>(*find(st(), "Lamp")));
  CHECK(blob()->primitives[0].material);
  CHECK(st().skeletons->size() == 1);
  CHECK(x_of(node, "Bone") == 7.f);
  CHECK(blob()->morph_weights[0] == 0.5f);
}

// The renderer packs a skin that a mesh points at even when scene_state does
// not list it (flattenScene's FlatScene::skins), so Skinning off has to take
// the skin off the meshes too, or the mesh is still skinned.
TEST_CASE("live link skinning off reaches the renderer unskinned", "[livelink]")
{
  GltfLiveLink node;
  quiet(node);
  auto scene = make_rich_scene();
  {
    // Blob's mesh skinned by the scene's skeleton, as GltfParser emits it.
    auto roots = *scene.state->roots;
    auto& blob_node = roots[3];
    auto children = *blob_node->children;
    for(auto& c : children)
      if(auto* m = ossia::get_if<ossia::mesh_component_ptr>(&c))
      {
        auto skinned = std::make_shared<ossia::mesh_component>(**m);
        skinned->skin = (*scene.state->skeletons)[0];
        *m = std::move(skinned);
      }
    auto n = std::make_shared<ossia::scene_node>(*blob_node);
    n->children = std::make_shared<const std::vector<ossia::scene_payload>>(std::move(children));
    blob_node = std::move(n);
    scene.state->roots
        = std::make_shared<std::vector<ossia::scene_node_ptr>>(std::move(roots));
  }
  node.set_scene(scene.state, 1, {"Mat"});

  auto flatten = [&] {
    ossia::scene_spec spec;
    spec.state = node.outputs.scene_out.scene.state;
    score::gfx::FlatScene flat;
    score::gfx::flattenScene(spec, flat, 1.f);
    return flat;
  };
  auto blob = [&] {
    return component<ossia::mesh_component>(*find(*node.outputs.scene_out.scene.state, "Blob"));
  };

  node();
  {
    const auto flat = flatten();
    REQUIRE(flat.draws.size() == 1);
    CHECK(flat.draws[0].skinIndex == 0);
    CHECK(flat.skins.size() == 1);
  }

  // Materials off first: the mesh comes out without its material but skinned.
  node.inputs.materials.value = false;
  node();
  CHECK_FALSE(blob()->primitives[0].material);
  CHECK(blob()->skin);

  // Then Skinning off as well, and alone: no skin reaches the renderer.
  node.inputs.skinning.value = false;
  node();
  CHECK_FALSE(blob()->skin);
  node.inputs.materials.value = true;
  node();
  CHECK(blob()->primitives[0].material);
  CHECK_FALSE(blob()->skin);
  {
    const auto flat = flatten();
    REQUIRE(flat.draws.size() == 1);
    CHECK(flat.draws[0].skinIndex == -1);
    CHECK(flat.skins.empty());
  }

  node.inputs.skinning.value = true;
  node();
  CHECK(blob()->skin);
  CHECK(flatten().draws[0].skinIndex == 0);
}

TEST_CASE("live link reads Blender files with negative texCoord indices", "[livelink]")
{
  // What Blender's exporter writes for a texture whose UV Map node names a UV
  // map the mesh does not have.
  const auto glb = triangle_glb(
      R"("materials":[{"name":"M","pbrMetallicRoughness":{"baseColorTexture":{"index":0,"texCoord": -1}}}],)"
      R"("samplers":[{}],"images":[{"uri":"missing.png"}],"textures":[{"sampler":0,"source":0}],)",
      0);
  GltfLiveLink node;
  quiet(node);
  LiveLink::inbox box;
  box.scene = LiveLink::scene_message{1, glb};
  node.apply(std::move(box));
  node();
  REQUIRE(node.outputs.scene_out.scene.state);
  CHECK(find(*node.outputs.scene_out.scene.state, "tri"));
}

namespace
{
// A unit quad (two triangles, not indexed) with normals and UVs.
native_primitive quad(float z = 0.f)
{
  native_primitive p;
  p.vertex_count = 6;
  p.positions = std::make_shared<std::vector<float>>(std::vector<float>{
      0, 0, z, 1, 0, z, 1, 1, z, 0, 0, z, 1, 1, z, 0, 1, z});
  p.normals = std::make_shared<std::vector<float>>(std::vector<float>(18, 0.f));
  for(int v = 0; v < 6; ++v)
    (*p.normals)[3 * v + 2] = 1.f;
  p.texcoords = std::make_shared<std::vector<float>>(std::vector<float>{
      0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0});
  return p;
}

LiveLink::structural_message mesh_msg(uint64_t seq, std::string key, native_primitive p)
{
  auto m = parse_mesh(encode_mesh(seq, key, {p}).substr(5));
  REQUIRE(m);
  LiveLink::structural_message out{.seq = seq, .type = frame_type::mesh};
  out.mesh = std::move(*m);
  return out;
}

LiveLink::structural_message image_msg(uint64_t seq, std::string key)
{
  auto m = parse_image(encode_image(seq, key, "image/png", "\x89PNG-not-really").substr(5));
  REQUIRE(m);
  return {.seq = seq, .type = frame_type::image, .image = std::move(*m)};
}

LiveLink::structural_message scene_msg(uint64_t seq, bool reset, std::string json)
{
  auto m = parse_native_scene(encode_native_scene(seq, reset, json).substr(5));
  REQUIRE(m);
  return {.seq = seq, .type = frame_type::native_scene, .scene = std::move(*m)};
}

const char* box_json = R"({
  "materials": {"Paint": {"base_color": [1, 0.5, 0.25, 1], "metallic": 0, "roughness": 0.4,
    "double_sided": true, "textures": {"base_color": {"image": "img:wood", "uv": 0}}}},
  "nodes": [
    {"name": "Box", "parent": -1, "t": [1, 2, 3], "r": [0, 0, 0, 1], "s": [1, 1, 1],
     "mesh": "mesh:quad", "materials": ["Paint"]},
    {"name": "Lamp", "parent": 0, "t": [0, 1, 0], "r": [0, 0, 0, 1], "s": [1, 1, 1],
     "light": [1, 1, 0.5, 0.25, 40, 0, 0, 0.785]}
  ]})";
}

TEST_CASE("live link native frames round-trip, and reject bad input", "[livelink]")
{
  auto p = quad();
  p.indices = std::make_shared<std::vector<uint32_t>>(std::vector<uint32_t>{0, 1, 2, 3, 4, 5});
  const auto frame = encode_mesh(7, "mesh:quad", {p});
  auto m = parse_mesh(frame.substr(5));
  REQUIRE(m);
  CHECK(m->seq == 7);
  CHECK(m->key == "mesh:quad");
  REQUIRE(m->primitives.size() == 1);
  CHECK(m->primitives[0].vertex_count == 6);
  CHECK((*m->primitives[0].positions)[6] == 1.f);
  CHECK(m->primitives[0].texcoords);
  CHECK_FALSE(m->primitives[0].colors);
  CHECK(m->primitives[0].indices->size() == 6);

  // Every truncation is refused, as is an index past the vertices.
  const std::string_view body{frame.data() + 5, frame.size() - 5};
  for(std::size_t n = 0; n < body.size(); n += 7)
    CHECK_FALSE(parse_mesh(body.substr(0, n)));
  auto bad = p;
  bad.indices = std::make_shared<std::vector<uint32_t>>(std::vector<uint32_t>{0, 1, 6});
  CHECK_FALSE(parse_mesh(encode_mesh(1, "k", {bad}).substr(5)));
  // Not indexed: a triangle list.
  auto odd = quad();
  odd.vertex_count = 5;
  odd.positions->resize(15);
  odd.normals.reset();
  odd.texcoords.reset();
  CHECK_FALSE(parse_mesh(encode_mesh(1, "k", {odd}).substr(5)));
}

TEST_CASE("live link builds native scenes, and keeps their order", "[livelink]")
{
  GltfLiveLink node;
  quiet(node);
  auto send = [&](auto... msgs) {
    LiveLink::inbox b;
    (b.structural.push_back(std::move(msgs)), ...);
    node.apply(std::move(b));
    node();
  };
  auto state = [&]() -> const ossia::scene_state& { return *node.outputs.scene_out.scene.state; };

  // Resources first, then the scene that uses them: the reset, newer than
  // they are, must not drop them.
  send(image_msg(1, "img:wood"), mesh_msg(2, "mesh:quad", quad()), scene_msg(3, true, box_json));
  const auto* box = find(state(), "Box");
  REQUIRE(box);
  const auto* mesh = component<ossia::mesh_component>(*box);
  REQUIRE(mesh);
  REQUIRE(mesh->primitives.size() == 1);
  const auto& prim = mesh->primitives[0];
  CHECK(prim.vertex_count == 6);
  CHECK(prim.bounds.max[0] == 1.f);
  // Tangents were generated, as GltfParser generates them.
  bool tangents = false;
  for(const auto& a : prim.attributes)
    tangents |= a.semantic == ossia::attribute_semantic::tangent;
  CHECK(tangents);
  REQUIRE(prim.material);
  CHECK(prim.material->tag == "Paint");
  CHECK(prim.material->base_color_factor[1] == 0.5f);
  CHECK(prim.material->roughness_factor == Catch::Approx(0.4f));
  CHECK(prim.material->double_sided);
  REQUIRE(prim.material->base_color_texture.source);
  CHECK(prim.material->base_color_texture.source->mime_type == "image/png");
  CHECK(prim.material->base_color_texture.source->embedded_data->size() == 15);
  // The light is under its parent.
  const auto* lamp = find(state(), "Lamp");
  REQUIRE(lamp);
  const auto* light = component<ossia::light_component>(*lamp);
  REQUIRE(light);
  CHECK(light->intensity == 40.f);
  CHECK(light->color[1] == 0.5f);
  CHECK(x_of(node, "Box") == 1.f);

  // Values stream to native nodes and materials as to glTF ones.
  {
    LiveLink::inbox b;
    put_record(b.transforms, "Box", 4, record("Box", 9, 0, 0));
    material_record r;
    r.name = "Paint";
    r.fields = material_record::roughness_field;
    r.roughness = 0.9f;
    put_record(b.materials, "Paint", 4, r);
    node.apply(std::move(b));
    node();
  }
  CHECK(x_of(node, "Box") == 9.f);
  CHECK((*state().materials)[0]->roughness_factor == Catch::Approx(0.9f));

  // A native patch adds a copy using the same mesh: the same upload.
  send(scene_msg(5, false, R"({"materials": {}, "nodes": [
      {"name": "Box2", "t": [0, 0, 0], "r": [0, 0, 0, 1], "s": [1, 1, 1], "mesh": "mesh:quad"}]})"));
  const auto* box2 = find(state(), "Box2");
  REQUIRE(box2);
  CHECK(find(state(), "Box"));
  const auto* mesh2 = component<ossia::mesh_component>(*box2);
  REQUIRE(mesh2);
  CHECK(mesh2->primitives[0].stable_id == component<ossia::mesh_component>(*find(state(), "Box"))->primitives[0].stable_id);
  CHECK_FALSE(mesh2->primitives[0].material);

  // A reset replaces everything.
  send(scene_msg(6, true, R"({"nodes": [{"name": "Only", "t": [0, 0, 0], "r": [0, 0, 0, 1], "s": [1, 1, 1]}]})"));
  CHECK(find(state(), "Only"));
  CHECK_FALSE(find(state(), "Box"));
  CHECK_FALSE(find(state(), "Box2"));

  // Invalid JSON changes nothing.
  send(scene_msg(7, true, "{not json"));
  CHECK(find(state(), "Only"));

  SECTION("a scene waits for the tangents of the meshes before it")
  {
    std::vector<GltfLiveLink::job> requests;
    node.worker.request = [&](GltfLiveLink::job j) { requests.push_back(std::move(j)); };
    send(mesh_msg(8, "mesh:quad", quad()), scene_msg(9, true, box_json));
    REQUIRE(requests.size() == 1);
    REQUIRE(std::holds_alternative<GltfLiveLink::mesh_job>(requests.front()));
    // Not applied yet: the box would have had no mesh.
    CHECK(find(state(), "Only"));
    CHECK_FALSE(find(state(), "Box"));

    auto done = GltfLiveLink::worker::work(std::move(requests.front()));
    REQUIRE(done);
    done(node);
    node();
    CHECK_FALSE(find(state(), "Only"));
    const auto* box = find(state(), "Box");
    REQUIRE(box);
    const auto* mesh = component<ossia::mesh_component>(*box);
    REQUIRE(mesh);
    bool tangents = false;
    for(const auto& a : mesh->primitives[0].attributes)
      tangents |= a.semantic == ossia::attribute_semantic::tangent;
    CHECK(tangents);
  }
}

TEST_CASE("live link answers a hello with what it accepts", "[livelink]")
{
  namespace asio = boost::asio;
  using tcp = asio::ip::tcp;
  asio::io_context ctx;
  uint16_t port{};
  {
    tcp::acceptor probe{ctx, tcp::endpoint{asio::ip::make_address("127.0.0.1"), 0}};
    port = probe.local_endpoint().port();
  }
  GltfLiveLink node;
  node.inputs.port.value = port;
  node.inputs.bind.value = "127.0.0.1";
  node.inputs.listen.value = true;
  node();

  tcp::socket sock{ctx};
  sock.connect(tcp::endpoint{asio::ip::make_address("127.0.0.1"), port});
  asio::write(sock, asio::buffer(encode_frame(
      frame_type::hello, R"({"protocol":2,"app":"test","features":["scene"]})")));

  uint32_t len{};
  asio::read(sock, asio::buffer(&len, 4));
  REQUIRE(len > 1);
  REQUIRE(len < 4096);
  std::string rest(len, '\0');
  asio::read(sock, asio::buffer(rest));
  CHECK(uint8_t(rest[0]) == uint8_t(frame_type::hello));
  const auto doc = QJsonDocument::fromJson(QByteArray(rest.data() + 1, int(rest.size() - 1)));
  const auto o = doc.object();
  CHECK(o["protocol"].toInt() == LiveLink::protocol_version);
  QStringList accepts;
  for(const auto& v : o["accepts"].toArray())
    accepts << v.toString();
  CHECK(accepts.contains("scene"));
  CHECK(accepts.contains("native_scene"));
  CHECK(accepts.contains("mesh"));
}

TEST_CASE("live link native meshes carry skins and shape keys", "[livelink]")
{
  auto p = quad();
  p.joints = std::make_shared<std::vector<uint16_t>>(std::vector<uint16_t>(24, 0));
  p.weights = std::make_shared<std::vector<float>>(std::vector<float>(24, 0.f));
  for(int v = 0; v < 6; ++v)
    (*p.weights)[4 * v] = 1.f, (*p.joints)[4 * v] = uint16_t(v % 2);
  native_primitive::morph t;
  t.positions = std::make_shared<std::vector<float>>(std::vector<float>(18, 0.f));
  (*t.positions)[3 * 1] = 2.f; // vertex 1 moves +2 in x
  p.targets.push_back(t);

  auto m = parse_mesh(encode_mesh(1, "mesh:skinned", {p}).substr(5));
  REQUIRE(m);
  REQUIRE(m->primitives[0].joints);
  CHECK((*m->primitives[0].joints)[4] == 1);
  REQUIRE(m->primitives[0].targets.size() == 1);
  CHECK((*m->primitives[0].targets[0].positions)[3] == 2.f);

  GltfLiveLink node;
  quiet(node);
  LiveLink::inbox b;
  LiveLink::structural_message mm{.seq = 1, .type = frame_type::mesh};
  mm.mesh = std::move(*m);
  b.structural.push_back(std::move(mm));
  // Rig (0) > Root (1) > Tip (2); Skin (3), under the rig, skinned to it;
  // Blob (4) with the shape key half on.
  b.structural.push_back(scene_msg(2, true, R"({"materials": {}, "nodes": [
    {"name": "Rig", "parent": -1, "t": [0, 0, 0], "r": [0, 0, 0, 1], "s": [1, 1, 1]},
    {"name": "Root", "parent": 0, "t": [0, 1, 0], "r": [0, 0, 0, 1], "s": [1, 1, 1]},
    {"name": "Tip", "parent": 1, "t": [0, 1, 0], "r": [0, 0, 0, 1], "s": [1, 1, 1]},
    {"name": "Skin", "parent": 0, "t": [0, 0, 0], "r": [0, 0, 0, 1], "s": [1, 1, 1],
     "mesh": "mesh:skinned",
     "skin": {"armature": 0, "joints": ["Root", "Tip"],
              "inverse_bind": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,-1,0,1,  1,0,0,0, 0,1,0,0, 0,0,1,0, 0,-2,0,1]}},
    {"name": "Blob", "parent": -1, "t": [5, 0, 0], "r": [0, 0, 0, 1], "s": [1, 1, 1],
     "mesh": "mesh:skinned", "weights": [0.5]}]})"));
  node.apply(std::move(b));
  node();
  const auto& st = *node.outputs.scene_out.scene.state;

  REQUIRE(st.skeletons);
  REQUIRE(st.skeletons->size() == 1);
  const auto& skel = *(*st.skeletons)[0];
  REQUIRE(skel.joints.size() == 2); // the rig is the mesh's frame: no extra root
  CHECK(skel.joints[0].name == "Root");
  CHECK(skel.joints[0].parent_index == -1);
  CHECK(skel.joints[1].parent_index == 0);
  CHECK(skel.joints[1].translation[1] == 1.f);
  CHECK(skel.joints[1].inverse_bind_matrix[13] == -2.f);
  CHECK(skel.joint_node_ids[0].value == find(st, "Root")->id.value);
  const auto* skin_mesh = component<ossia::mesh_component>(*find(st, "Skin"));
  REQUIRE(skin_mesh);
  CHECK(skin_mesh->skin.get() == (*st.skeletons)[0].get());
  bool joints = false;
  for(const auto& a : skin_mesh->primitives[0].attributes)
    joints |= a.semantic == ossia::attribute_semantic::joints0;
  CHECK(joints);

  // The shape key at its weight: vertex 1 half-way, +1 in x.
  const auto* blob = component<ossia::mesh_component>(*find(st, "Blob"));
  REQUIRE(blob);
  REQUIRE(blob->morph_targets.size() == 1);
  CHECK(blob->morph_weights[0] == 0.5f);
  CHECK(floats_of(blob->primitives[0].vertex_buffers[0])[3] == 2.f);

  // Bones stream to native rigs as to glTF ones.
  {
    LiveLink::inbox b2;
    put_record(b2.joints, joint_key("Rig", "Tip"), 3, joint_record{"Rig", record("Tip", 0, 3, 0)});
    node.apply(std::move(b2));
    node();
  }
  const auto& st2 = *node.outputs.scene_out.scene.state;
  CHECK((*st2.skeletons)[0]->joints[1].translation[1] == 3.f);
}

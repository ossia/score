// Scene-object contracts.
//
// Pure logic -- these nodes are compiled into the test and no QRhi handle is
// ever dereferenced, so the cases say what the node computes rather than what
// a GPU draws.
//
// The two animation cases (STEP / CUBICSPLINE, and the skinning one) are
// deliberately absent: the available animation fixture failed its own
// before-operation precondition (before.draws.size() was zero, not one), so it
// proves nothing about skinning and needs re-deriving before it is imported.
#include <Threedim/Instancer.hpp>
#include <Threedim/CameraSwitch.hpp>
#include <Threedim/BufferToGeometry.cpp>
#include <Threedim/BufferToGeometry2.cpp>
#include <Gfx/Graph/SceneGPUState.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <QMatrix4x4>
#include <QQuaternion>

namespace review_scene_objects {
using Catch::Approx;
ossia::mesh_primitive triangle() {
  auto xyz = std::make_shared<std::vector<float>>(
    std::initializer_list<float>{0,0,0, 1,0,0, 0,1,0});
  auto buffer = std::make_shared<ossia::buffer_resource>();
  buffer->resource = ossia::buffer_data{
    .data = std::shared_ptr<const void>(xyz, xyz->data()), .byte_size = 36};
  ossia::mesh_primitive prim;
  prim.vertex_count = 3; prim.vertex_buffers.push_back(buffer);
  ossia::vertex_attribute attr;
  attr.semantic = ossia::attribute_semantic::position;
  attr.format = ossia::vertex_format::float3;
  attr.buffer_index = 0; attr.byte_offset = 0; attr.byte_stride = 12;
  prim.attributes.push_back(attr);
  return prim;
}
std::shared_ptr<ossia::scene_state> prototype(float sx = 1.f) {
  auto st = std::make_shared<ossia::scene_state>();
  auto root = std::make_shared<ossia::scene_node>();
  root->id.value = 77;
  ossia::scene_transform xf; xf.scale[0] = sx;
  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->primitives.push_back(triangle());
  root->children = std::make_shared<const std::vector<ossia::scene_payload>>(
    std::vector<ossia::scene_payload>{xf, ossia::mesh_component_ptr(mesh)});
  st->roots = std::make_shared<const std::vector<ossia::scene_node_ptr>>(
    std::vector<ossia::scene_node_ptr>{root});
  return st;
}
const ossia::instance_component& instance(const Threedim::Instancer& n) {
  const auto& r = *(*n.m_wrapped_state->roots)[0];
  for (auto& p : *r.children)
    if (auto* q = ossia::get_if<ossia::instance_component_ptr>(&p)) return **q;
  throw std::runtime_error("no instance output");
}
template<class Node> void configureBuffers(Node& n, void* handle) {
  n.inputs.attribute_buffer_0.value = 0;
  n.inputs.format_0.value = Threedim::AttributeFormat::Float3;
  n.inputs.vertices.value = 3;
  n.inputs.instances.value = 1;
  n.inputs.index_buffer.value = -1;
  n.inputs.buffer_0.buffer.handle = handle;
  n.inputs.buffer_0.buffer.byte_size = 128;
}
template<class Node> void checkBufferControls() {
  int opaque{}; Node n; configureBuffers(n, &opaque); n();
  REQUIRE(n.outputs.geometry.mesh.instances == 1);
  n.inputs.instances.value = 5; n();
  CHECK(n.outputs.geometry.mesh.instances == 5);
  n.inputs.buffer_0.buffer.byte_offset = 32; n();
  CHECK(n.outputs.geometry.mesh.input[0].byte_offset == 32);
  n.inputs.buffer_0.buffer.byte_size = 64; n();
  CHECK(n.outputs.geometry.mesh.buffers[0].byte_size == 64);
  // Invalidating the cached structural fingerprint must not lose the control
  // values.
  n.m_prevVertices = -1; n();
  CHECK(n.outputs.geometry.mesh.instances == 5);
  CHECK(n.outputs.geometry.mesh.input[0].byte_offset == 32);
  CHECK(n.outputs.geometry.mesh.buffers[0].byte_size == 64);
}
ossia::scene_spec camera(ossia::camera_projection projection) {
  auto st = std::make_shared<ossia::scene_state>();
  auto cam = std::make_shared<ossia::camera_component>(); cam->projection = projection;
  auto root = std::make_shared<ossia::scene_node>();
  root->children = std::make_shared<const std::vector<ossia::scene_payload>>(
    std::vector<ossia::scene_payload>{ossia::scene_transform{}, ossia::camera_component_ptr(cam)});
  st->roots = std::make_shared<const std::vector<ossia::scene_node_ptr>>(
    std::vector<ossia::scene_node_ptr>{root});
  ossia::scene_spec out; out.state = st; return out;
}
}
using namespace review_scene_objects;

TEST_CASE("SceneObjects-01 both geometry nodes respond to Instances and same-handle buffer views", "[SceneObjects][buffers]") {
  SECTION("v1") { checkBufferControls<Threedim::BuffersToGeometry>(); }
  SECTION("v2") { checkBufferControls<Threedim::BuffersToGeometry2>(); }
}
TEST_CASE("SceneObjects-02 a custom semantic keeps its authored name", "[SceneObjects][buffers]") {
  // `temperature` is a first-class semantic in ossia
  // (attribute_semantic::temperature == 1401, right after density == 1400), so
  // name_to_semantic resolves it and halp's contract -- "name: For custom
  // semantics; empty = use semantic name" -- makes an empty name the correct
  // outcome. Both halves of that contract are checked below.
  int opaque{};
  SECTION("a recognised semantic resolves and needs no name")
  {
    Threedim::BuffersToGeometry2 n; configureBuffers(n, &opaque);
    n.inputs.semantic_0.value = "temperature"; n();
    REQUIRE(n.outputs.geometry.mesh.attributes.size() == 1);
    CHECK(n.outputs.geometry.mesh.attributes[0].semantic
          == static_cast<halp::attribute_semantic>(
              ossia::attribute_semantic::temperature));
    CHECK(n.outputs.geometry.mesh.attributes[0].name.empty());
  }
  SECTION("an unrecognised semantic keeps its text, or it is anonymous")
  {
    // Without this, every unrecognised attribute lands on `custom` with no
    // name -- indistinguishable from every other one, and a shader asking for
    // this attribute by name has nothing to match against.
    Threedim::BuffersToGeometry2 n; configureBuffers(n, &opaque);
    n.inputs.semantic_0.value = "vorticity_magnitude"; n();
    REQUIRE(n.outputs.geometry.mesh.attributes.size() == 1);
    CHECK(n.outputs.geometry.mesh.attributes[0].semantic
          == static_cast<halp::attribute_semantic>(
              ossia::attribute_semantic::custom));
    CHECK(n.outputs.geometry.mesh.attributes[0].name == "vorticity_magnitude");
  }
}
TEST_CASE("SceneObjects-05 Instancer notices same-handle view shrink", "[SceneObjects][instancer]") {
  Threedim::Instancer n; int opaque{};
  n.inputs.scene_in.scene.state = prototype();
  n.inputs.transforms.buffer.handle = &opaque;
  n.inputs.transforms.buffer.byte_size = 128;
  n.inputs.count.value = 2; n(); REQUIRE(instance(n).instance_count == 2);
  n.inputs.transforms.buffer.byte_size = 64; n();
  CHECK(instance(n).instance_count == 1);
  n.rebuild(); // a descriptor rebuild applies the same clamp
  CHECK(instance(n).instance_count == 1);
}
TEST_CASE("SceneObjects-06 Instancer accepts tightly packed XYZ Points", "[SceneObjects][instancer]") {
  Threedim::Instancer n; int opaque{};
  n.inputs.scene_in.scene.state = prototype();
  auto& g = n.inputs.points.mesh;
  g.vertices = 2;
  g.buffers.push_back({.handle=&opaque, .byte_size=24, .dirty=true});
  g.bindings.push_back({.stride=12, .step_rate=1,
    .classification=halp::binding_classification::per_vertex});
  g.attributes.push_back({.binding=0, .semantic=halp::attribute_semantic::position,
    .format=halp::attribute_format::float3, .byte_offset=0});
  g.input.push_back({.buffer=0, .byte_offset=0});
  n();
  CHECK(instance(n).instance_count == 2);
}
TEST_CASE("SceneObjects-07 Instancer retains reflected prototype transform", "[SceneObjects][instancer]") {
  Threedim::Instancer n;
  n.inputs.scene_in.scene.state = prototype(-1.f); n();
  const auto& children = *(*n.m_wrapped_state->roots)[0]->children;
  auto& t = ossia::get<ossia::scene_transform>(children[1]);
  QMatrix4x4 m; m.translate(t.translation[0],t.translation[1],t.translation[2]);
  m.rotate(QQuaternion(t.rotation[3],t.rotation[0],t.rotation[1],t.rotation[2]));
  m.scale(t.scale[0],t.scale[1],t.scale[2]);
  CHECK(m.map(QVector3D(1,0,0)).x() == Approx(-1.f));
}
TEST_CASE("SceneObjects-08 one weighted orthographic camera stays orthographic without camera zero", "[SceneObjects][camera]") {
  Threedim::CameraSwitch n;
  n.inputs.mode.value = Threedim::CameraSwitch::ins::CameraMode::Blend;
  n.inputs.cam1.scene = camera(ossia::camera_projection::orthographic);
  n.inputs.weights.value = {0,1,0,0}; n.rebuild(); n();
  ossia::scene_transform xf; ossia::camera_component cam;
  REQUIRE(Threedim::CameraSwitch::extractCameraPose(n.outputs.scene_out.scene, xf, cam));
  CHECK(cam.projection == ossia::camera_projection::orthographic);
}


TEST_CASE(
    "SceneObjects-09 a camera published without any root node still reaches the scene",
    "[SceneObjects][camera]")
{
  // scene_state carries roots, cameras, materials and skeletons as independent
  // vectors, but scene_state::empty() reports on `roots` alone. flattenScene
  // must not bail on that predicate: a producer publishing only a camera --
  // which SceneGPUState.cpp's own camera block calls out as supported,
  // "producers that don't want to embed a camera node can publish via
  // `cameras` only" -- would have its whole scene dropped and the consumer
  // would fall back to the default eye.
  auto st = std::make_shared<ossia::scene_state>();
  auto cam = std::make_shared<ossia::camera_component>();
  cam->projection = ossia::camera_projection::orthographic;
  st->cameras = std::make_shared<const std::vector<ossia::camera_component_ptr>>(
      std::vector<ossia::camera_component_ptr>{cam});
  // deliberately no roots: that is the whole point of the case
  REQUIRE(!st->roots);
  REQUIRE(st->empty()); // the predicate flattenScene must not stop on

  ossia::scene_spec spec;
  spec.state = st;

  score::gfx::FlatScene out;
  score::gfx::flattenScene(spec, out, 1.f);

  INFO("cameras reaching the flattened scene: " << out.cameras.size());
  CHECK(out.cameras.size() == 1);
  CHECK(out.activeCameraIndex == 0);
}

TEST_CASE(
    "SceneObjects-10 an empty scene_state is still dropped",
    "[SceneObjects][camera]")
{
  // The control for the case above: relaxing the guard must not turn "nothing
  // published" into work. A state with no roots, no cameras, no materials and
  // no skeletons still returns immediately.
  auto st = std::make_shared<ossia::scene_state>();
  ossia::scene_spec spec;
  spec.state = st;

  score::gfx::FlatScene out;
  score::gfx::flattenScene(spec, out, 1.f);

  CHECK(out.cameras.empty());
  CHECK(out.draws.empty());
}

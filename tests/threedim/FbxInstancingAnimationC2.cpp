// FBX: nodes that instance one geometry share its primitive identity, and FBX
// node TRS animation reaches AnimationPlayer.
//
// c2-fbx-shared-mesh.fbx: models "Left" and "Right" both connect to one
// triangle geometry. The loader extracted the geometry once per node with a
// fresh mesh_primitive stable_id, so the scene preprocessor uploaded a copy
// per instance.
//
// c2-fbx-anim-node.fbx: model "Mover", one take of 2 s, LINEAR keys on
// Lcl Translation X (0 -> 4) and Lcl Rotation Y (0 -> 90 degrees). The loader
// imported no animation, so AnimationPlayer passed the scene through. Sampled
// at t = 0 and t = 1 the node must sit at x = 0 / x = 2, turned 0 / 45 degrees
// about Y.
#include <Threedim/AnimationPlayer.hpp>
#include <Threedim/FbxParser.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <vector>

namespace
{
std::shared_ptr<const ossia::scene_state> load(const std::string& path)
{
  using fbx_port = Threedim::FbxParser::ins::fbx_t;
  fbx_port::file_type file{};
  file.filename = path;
  auto apply = fbx_port::process(file);
  if(!apply)
    return {};
  Threedim::FbxParser parser;
  apply(parser);
  return parser.m_raw_state;
}

void collect(const ossia::scene_node& n, std::vector<const ossia::mesh_primitive*>& out)
{
  if(!n.children)
    return;
  for(const auto& p : *n.children)
  {
    if(auto* mc = ossia::get_if<ossia::mesh_component_ptr>(&p); mc && *mc)
      for(const auto& prim : (*mc)->primitives)
        out.push_back(&prim);
    if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&p); sub && *sub)
      collect(**sub, out);
  }
}

const ossia::scene_node* findByName(const ossia::scene_node& n, const std::string& name)
{
  if(n.name == name)
    return &n;
  if(!n.children)
    return nullptr;
  for(const auto& p : *n.children)
    if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&p); sub && *sub)
      if(auto* found = findByName(**sub, name))
        return found;
  return nullptr;
}

const ossia::scene_transform*
nodeTransform(const std::shared_ptr<const ossia::scene_state>& st, const std::string& name)
{
  if(!st || !st->roots)
    return nullptr;
  for(const auto& r : *st->roots)
  {
    if(!r)
      continue;
    if(auto* n = findByName(*r, name); n && n->children)
      for(const auto& p : *n->children)
        if(auto* xf = ossia::get_if<ossia::scene_transform>(&p))
          return xf;
  }
  return nullptr;
}
}

TEST_CASE("FBX nodes instancing one geometry share its primitive id", "[threedim][fbx]")
{
  const auto scene = load(THREEDIM_TEST_DIR "/c2-fbx-shared-mesh.fbx");
  REQUIRE(scene);
  REQUIRE(scene->roots);

  std::vector<const ossia::mesh_primitive*> prims;
  for(const auto& r : *scene->roots)
    if(r)
      collect(*r, prims);

  REQUIRE(prims.size() == 2);
  CHECK(prims[0]->stable_id != 0);
  CHECK(prims[0]->stable_id == prims[1]->stable_id);
  CHECK(prims[0]->vertex_count == 3);
  CHECK(prims[1]->vertex_count == 3);
}

TEST_CASE("FBX node animation drives AnimationPlayer", "[threedim][fbx][animation]")
{
  const auto scene = load(THREEDIM_TEST_DIR "/c2-fbx-anim-node.fbx");
  REQUIRE(scene);
  REQUIRE(scene->animations);
  REQUIRE(scene->animations->size() == 1);
  CHECK((*scene->animations)[0]->duration == Catch::Approx(2.f));

  Threedim::AnimationPlayer player;
  player.inputs.scene_in.scene.state = scene;
  player.inputs.speed.value = 0.f;
  player.inputs.loop.value = false;
  player.inputs.clip_index.value = 0;

  player.inputs.time.value = 0.f;
  player();
  const auto at0 = player.outputs.scene_out.scene.state;
  const auto* xf0 = nodeTransform(at0, "Mover");
  REQUIRE(xf0);
  CHECK(xf0->translation[0] == Catch::Approx(0.f).margin(1e-4));
  CHECK(xf0->rotation[1] == Catch::Approx(0.f).margin(1e-4));
  CHECK(std::abs(xf0->rotation[3]) == Catch::Approx(1.f).margin(1e-4));

  player.inputs.time.value = 1.f;
  player();
  const auto at1 = player.outputs.scene_out.scene.state;
  const auto* xf1 = nodeTransform(at1, "Mover");
  REQUIRE(xf1);
  const float half = 22.5f * float(M_PI) / 180.f;
  CHECK(xf1->translation[0] == Catch::Approx(2.f).margin(1e-3));
  CHECK(xf1->translation[1] == Catch::Approx(0.f).margin(1e-4));
  CHECK(xf1->translation[2] == Catch::Approx(0.f).margin(1e-4));
  CHECK(xf1->rotation[0] == Catch::Approx(0.f).margin(1e-4));
  CHECK(xf1->rotation[1] == Catch::Approx(std::sin(half)).margin(1e-3));
  CHECK(xf1->rotation[2] == Catch::Approx(0.f).margin(1e-4));
  CHECK(xf1->rotation[3] == Catch::Approx(std::cos(half)).margin(1e-3));
}

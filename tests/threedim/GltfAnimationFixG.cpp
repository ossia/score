// A glTF animation reaches AnimationPlayer.
//
// fixg-anim-box.gltf is the gallery's anim_box.gltf: one node "AnimBox" and one
// animation "slide_turn", LINEAR translation (-1.2,0,0) -> (0,0.6,0) ->
// (1.2,0,0) and rotation 0 -> 90 -> 180 degrees about Y at t = 0, 1, 2. The
// loader published no animation_component, so AnimationPlayer passed the scene
// through and the box never moved. Parsed by GltfParser and sampled at t = 1,
// the node's transform must be (0, 0.6, 0), turned 90 degrees about Y.

#include <Threedim/AnimationPlayer.hpp>
#include <Threedim/GltfParser.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>

namespace
{
const ossia::scene_node* findNode(const ossia::scene_node& n, uint64_t id)
{
  if(n.id.value == id)
    return &n;
  if(!n.children)
    return nullptr;
  for(const auto& p : *n.children)
    if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&p))
      if(*sub)
        if(auto* found = findNode(**sub, id))
          return found;
  return nullptr;
}

const ossia::scene_transform*
nodeTransform(const std::shared_ptr<const ossia::scene_state>& st, uint64_t id)
{
  if(!st || !st->roots)
    return nullptr;
  for(const auto& r : *st->roots)
  {
    if(!r)
      continue;
    if(auto* n = findNode(*r, id); n && n->children)
      for(const auto& p : *n->children)
        if(auto* xf = ossia::get_if<ossia::scene_transform>(&p))
          return xf;
  }
  return nullptr;
}
}

TEST_CASE("glTF animations drive AnimationPlayer", "[threedim][gltf][animation]")
{
  using gltf_port = Threedim::GltfParser::ins::gltf_t;
  gltf_port::file_type file{};
  const std::string path = THREEDIM_TEST_DIR "/fixg-anim-box.gltf";
  file.filename = path;

  auto apply = gltf_port::process(file);
  REQUIRE(apply);
  Threedim::GltfParser parser;
  apply(parser);
  const auto scene = parser.m_raw_state;
  REQUIRE(scene);

  REQUIRE(scene->animations);
  REQUIRE(scene->animations->size() == 1);
  const auto& anim = *(*scene->animations)[0];
  CHECK(anim.channels.size() == 2);
  CHECK(anim.duration == Catch::Approx(2.f));

  Threedim::AnimationPlayer player;
  player.inputs.scene_in.scene.state = scene;
  player.inputs.time.value = 1.f;
  player.inputs.speed.value = 0.f;
  player.inputs.loop.value = false;
  player.inputs.clip_index.value = 0;
  player();

  // glTF node 0 is scene_node id 1.
  const auto* xf = nodeTransform(player.outputs.scene_out.scene.state, 1);
  REQUIRE(xf);
  const float s = std::sqrt(0.5f);
  CHECK(xf->translation[0] == Catch::Approx(0.f).margin(1e-4));
  CHECK(xf->translation[1] == Catch::Approx(0.6f).margin(1e-4));
  CHECK(xf->translation[2] == Catch::Approx(0.f).margin(1e-4));
  CHECK(std::abs(xf->rotation[1]) == Catch::Approx(s).margin(1e-3));
  CHECK(std::abs(xf->rotation[3]) == Catch::Approx(s).margin(1e-3));
}

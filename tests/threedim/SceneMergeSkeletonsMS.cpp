// ossia::merge_scenes carries every scene_state field a contributor can set
// when several scenes share one input: skeletons (AnimationPlayer's posed
// copies next to a Camera or Light), shadow cascades (ShadowCascadeSetup and
// SceneResourceRoute's shadow map array), injected buffers and textures, and
// the glTF material variant selection. The skeletons used to be dropped, so
// every clip rendered the meshes' rest skins.
#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <span>

namespace
{
std::shared_ptr<ossia::scene_state> withRoot(uint64_t id)
{
  auto node = std::make_shared<ossia::scene_node>();
  node->id = ossia::scene_node_id{id};
  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(std::move(node));
  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::move(roots);
  return st;
}

ossia::skeleton_component_ptr skeleton(float x)
{
  auto sk = std::make_shared<ossia::skeleton_component>();
  ossia::skeleton_joint j;
  j.translation[0] = x;
  sk->joints.push_back(j);
  sk->joint_node_ids.push_back(ossia::scene_node_id{1});
  return sk;
}

std::shared_ptr<ossia::scene_state> cameraOnly()
{
  auto st = std::make_shared<ossia::scene_state>();
  auto cams = std::make_shared<std::vector<ossia::camera_component_ptr>>();
  cams->push_back(std::make_shared<ossia::camera_component>());
  st->cameras = std::move(cams);
  st->active_camera_id = ossia::scene_node_id{77};
  return st;
}

ossia::scene_spec merge(std::initializer_list<ossia::scene_spec> in)
{
  return ossia::merge_scenes(std::span<const ossia::scene_spec>{in.begin(), in.size()});
}
}

TEST_CASE(
    "merge_scenes keeps the skeletons of a scene merged with a camera",
    "[threedim][scene][merge][skinning]")
{
  auto anim = withRoot(1);
  auto skels = std::make_shared<std::vector<ossia::skeleton_component_ptr>>();
  skels->push_back(skeleton(0.5f));
  skels->push_back(skeleton(2.f));
  anim->skeletons = skels;

  const auto m = merge({ossia::scene_spec{cameraOnly()}, ossia::scene_spec{anim}});
  REQUIRE(m.state);
  REQUIRE(m.state->cameras);
  CHECK(m.state->cameras->size() == 1);
  REQUIRE(m.state->skeletons);
  CHECK(m.state->skeletons == skels);
  REQUIRE(m.state->skeletons->size() == 2);
  CHECK((*m.state->skeletons)[0]->joints[0].translation[0] == 0.5f);
  CHECK((*m.state->skeletons)[1]->joints[0].translation[0] == 2.f);
}

TEST_CASE(
    "merge_scenes concatenates the skeletons of several contributors, once each",
    "[threedim][scene][merge][skinning]")
{
  auto shared = skeleton(1.f);
  auto a = withRoot(1);
  a->skeletons = std::make_shared<std::vector<ossia::skeleton_component_ptr>>(
      std::vector<ossia::skeleton_component_ptr>{shared, skeleton(3.f)});
  auto b = withRoot(2);
  b->skeletons = std::make_shared<std::vector<ossia::skeleton_component_ptr>>(
      std::vector<ossia::skeleton_component_ptr>{skeleton(4.f), shared});

  const auto m = merge({ossia::scene_spec{a}, ossia::scene_spec{b}});
  REQUIRE(m.state);
  REQUIRE(m.state->skeletons);
  REQUIRE(m.state->skeletons->size() == 3);
  CHECK((*m.state->skeletons)[0] == shared);
  CHECK((*m.state->skeletons)[1]->joints[0].translation[0] == 3.f);
  CHECK((*m.state->skeletons)[2]->joints[0].translation[0] == 4.f);
}

TEST_CASE(
    "merge_scenes overlays the shadow cascades and the shadow map array",
    "[threedim][scene][merge][shadow]")
{
  int tex = 0;
  auto route = std::make_shared<ossia::scene_state>();
  route->shadow_cascades.shadow_map_array.native_handle = &tex;

  auto setup = withRoot(1);
  setup->shadow_cascades.cascade_count = 3;
  setup->shadow_cascades.split_view_depths[3] = 42.f;
  setup->shadow_cascades.light_view_proj[2][5] = 7.f;
  setup->shadow_cascades.shadow_distance = 55.f;
  setup->shadow_cascades.light_direction[0] = 1.f;

  for(bool routeFirst : {true, false})
  {
    CAPTURE(routeFirst);
    const auto m = routeFirst
                       ? merge({ossia::scene_spec{route}, ossia::scene_spec{setup}})
                       : merge({ossia::scene_spec{setup}, ossia::scene_spec{route}});
    REQUIRE(m.state);
    const auto& sc = m.state->shadow_cascades;
    CHECK(sc.cascade_count == 3);
    CHECK(sc.split_view_depths[3] == 42.f);
    CHECK(sc.light_view_proj[2][5] == 7.f);
    CHECK(sc.shadow_distance == 55.f);
    CHECK(sc.light_direction[0] == 1.f);
    CHECK(sc.shadow_map_array.native_handle == &tex);
  }
}

TEST_CASE(
    "merge_scenes keeps injected buffers and textures, the later one per name",
    "[threedim][scene][merge][inject]")
{
  int b1 = 0, b2 = 0, b3 = 0, t1 = 0, t2 = 0;
  auto a = withRoot(1);
  a->inject_buffers.push_back({.name = "params", .native_handle = &b1, .byte_size = 16});
  a->inject_buffers.push_back({.name = "extra", .native_handle = &b2, .byte_size = 32});
  a->inject_textures.push_back({.name = "noise", .native_handle = &t1});
  auto b = cameraOnly();
  b->inject_buffers.push_back({.name = "params", .native_handle = &b3, .byte_size = 64});
  b->inject_textures.push_back({.name = "ramp", .native_handle = &t2});

  const auto m = merge({ossia::scene_spec{a}, ossia::scene_spec{b}});
  REQUIRE(m.state);
  REQUIRE(m.state->inject_buffers.size() == 2);
  CHECK(m.state->inject_buffers[0].name == "extra");
  CHECK(m.state->inject_buffers[0].native_handle == &b2);
  CHECK(m.state->inject_buffers[1].name == "params");
  CHECK(m.state->inject_buffers[1].native_handle == &b3);
  CHECK(m.state->inject_buffers[1].byte_size == 64);
  REQUIRE(m.state->inject_textures.size() == 2);
  CHECK(m.state->inject_textures[0].native_handle == &t1);
  CHECK(m.state->inject_textures[1].native_handle == &t2);
}

TEST_CASE(
    "merge_scenes keeps the material variant selection",
    "[threedim][scene][merge][variants]")
{
  auto gltf = withRoot(1);
  gltf->variant_names.push_back("day");
  gltf->variant_names.push_back("night");
  gltf->active_variant_index = 1;

  const auto m = merge({ossia::scene_spec{cameraOnly()}, ossia::scene_spec{gltf}});
  REQUIRE(m.state);
  CHECK(m.state->active_variant_index == 1);
  REQUIRE(m.state->variant_names.size() == 2);
  CHECK(m.state->variant_names[1] == "night");
  CHECK(m.state->active_camera_id.value == 77);
}

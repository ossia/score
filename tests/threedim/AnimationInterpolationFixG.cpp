// AnimationPlayer samples a node's TRS channels as glTF 2.0 defines them.
//
// The scene is what the glTF loader publishes for a one-node asset: a
// scene_node with id = node index + 1 whose first child is its local
// scene_transform. The first case is the gallery's anim_box.gltf ("slide_turn":
// LINEAR translation (-1.2,0,0) -> (0,0.6,0) -> (1.2,0,0) and rotation 0 -> 90
// -> 180 degrees about Y at t = 0, 1, 2). The two others pin the interpolation
// modes, which were all sampled as LINEAR over the raw value array: a STEP key
// holds its value until the next one, and a CUBICSPLINE key stores an
// in-tangent, a value and an out-tangent (Appendix C).

#include <Threedim/AnimationPlayer.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <vector>

namespace
{
constexpr uint64_t kNodeId = 1;

ossia::animation_channel channel(
    ossia::animation_target path, ossia::animation_interpolation interp,
    std::vector<float> times, std::vector<float> values)
{
  ossia::animation_channel ch;
  ch.target_node_id = kNodeId;
  ch.target_path = path;
  ch.interpolation = interp;
  ch.times = std::make_shared<std::vector<float>>(std::move(times));
  ch.values = std::make_shared<std::vector<float>>(std::move(values));
  return ch;
}

std::shared_ptr<ossia::scene_state>
makeScene(std::vector<ossia::animation_channel> channels, float duration)
{
  ossia::scene_transform rest{};
  rest.rotation[3] = 1.f;
  rest.scale[0] = rest.scale[1] = rest.scale[2] = 1.f;

  auto node = std::make_shared<ossia::scene_node>();
  node->id.value = kNodeId;
  node->children = std::make_shared<std::vector<ossia::scene_payload>>(
      std::vector<ossia::scene_payload>{rest});
  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(node);

  auto anim = std::make_shared<ossia::animation_component>();
  anim->duration = duration;
  for(auto& c : channels)
    anim->channels.push_back(std::move(c));
  auto anims = std::make_shared<std::vector<ossia::animation_component_ptr>>();
  anims->push_back(anim);

  auto st = std::make_shared<ossia::scene_state>();
  st->roots = roots;
  st->animations = anims;
  st->version = 1;
  return st;
}

ossia::scene_transform
sampleAt(const std::shared_ptr<ossia::scene_state>& scene, float time)
{
  Threedim::AnimationPlayer player;
  player.inputs.scene_in.scene.state = scene;
  player.inputs.time.value = time;
  player.inputs.speed.value = 0.f;
  player.inputs.loop.value = false;
  player.inputs.clip_index.value = 0;
  player();

  const auto& out = player.outputs.scene_out.scene.state;
  REQUIRE(out);
  REQUIRE(out->roots);
  REQUIRE(!out->roots->empty());
  const auto& node = (*out->roots)[0];
  REQUIRE(node);
  REQUIRE(node->children);
  REQUIRE(!node->children->empty());
  const auto* xf = ossia::get_if<ossia::scene_transform>(&(*node->children)[0]);
  REQUIRE(xf);
  return *xf;
}
}

TEST_CASE(
    "AnimationPlayer moves and turns anim_box.gltf's node",
    "[threedim][animation]")
{
  const float s = std::sqrt(0.5f);
  auto scene = makeScene(
      {channel(
           ossia::animation_target::translation,
           ossia::animation_interpolation::linear, {0.f, 1.f, 2.f},
           {-1.2f, 0.f, 0.f, 0.f, 0.6f, 0.f, 1.2f, 0.f, 0.f}),
       channel(
           ossia::animation_target::rotation, ossia::animation_interpolation::linear,
           {0.f, 1.f, 2.f}, {0.f, 0.f, 0.f, 1.f, 0.f, s, 0.f, s, 0.f, 1.f, 0.f, 0.f})},
      2.f);

  const auto t0 = sampleAt(scene, 0.f);
  CHECK(t0.translation[0] == Catch::Approx(-1.2f).margin(1e-5));

  const auto t1 = sampleAt(scene, 1.f);
  CHECK(t1.translation[0] == Catch::Approx(0.f).margin(1e-5));
  CHECK(t1.translation[1] == Catch::Approx(0.6f).margin(1e-5));
  CHECK(t1.rotation[1] == Catch::Approx(s).margin(1e-4));
  CHECK(t1.rotation[3] == Catch::Approx(s).margin(1e-4));

  const auto t2 = sampleAt(scene, 2.f);
  CHECK(t2.translation[0] == Catch::Approx(1.2f).margin(1e-5));
  CHECK(std::abs(t2.rotation[1]) == Catch::Approx(1.f).margin(1e-4));
}

TEST_CASE("AnimationPlayer holds a STEP key until the next one", "[threedim][animation]")
{
  auto scene = makeScene(
      {channel(
          ossia::animation_target::translation, ossia::animation_interpolation::step,
          {0.f, 1.f}, {0.f, 0.f, 0.f, 1.f, 0.f, 0.f})},
      1.f);

  CHECK(sampleAt(scene, 0.5f).translation[0] == Catch::Approx(0.f).margin(1e-6));
  CHECK(sampleAt(scene, 0.99f).translation[0] == Catch::Approx(0.f).margin(1e-6));
  CHECK(sampleAt(scene, 1.f).translation[0] == Catch::Approx(1.f).margin(1e-6));
}

TEST_CASE(
    "AnimationPlayer interpolates a CUBICSPLINE channel with its tangents",
    "[threedim][animation]")
{
  // Two keys at t = 0 and 1: values x = 0 and 1, out-tangent of the first
  // (2, 0, 0), in-tangent of the second 0. Per Appendix C, at t = 0.5:
  //   (-2t^3 + 3t^2) * 1 + (t^3 - 2t^2 + t) * 1 * 2 = 0.5 + 0.25 = 0.75
  auto scene = makeScene(
      {channel(
          ossia::animation_target::translation,
          ossia::animation_interpolation::cubic_spline, {0.f, 1.f},
          {0.f, 0.f, 0.f, /**/ 0.f, 0.f, 0.f, /**/ 2.f, 0.f, 0.f, //
           0.f, 0.f, 0.f, /**/ 1.f, 0.f, 0.f, /**/ 0.f, 0.f, 0.f})},
      1.f);

  CHECK(sampleAt(scene, 0.f).translation[0] == Catch::Approx(0.f).margin(1e-6));
  CHECK(sampleAt(scene, 0.5f).translation[0] == Catch::Approx(0.75f).margin(1e-5));
  CHECK(sampleAt(scene, 1.f).translation[0] == Catch::Approx(1.f).margin(1e-6));
}

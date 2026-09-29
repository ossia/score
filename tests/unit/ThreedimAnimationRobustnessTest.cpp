// Animations from untrusted files and inputs: the glTF loader refuses keyframe
// data it cannot play (a declared count backed by nothing in the file, times
// that are not ascending), and AnimationPlayer clamps instead of reading past
// a channel's keys when the time it is given is NaN.
//
// Pure CPU: the loader and the player, no QRhi.
#include <Threedim/AnimationPlayer.hpp>
#include <Threedim/GltfParser.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{
// A one-node glTF whose node 0 has a translation animation. `input` is the
// sampler's input accessor, `buffer` the base64 of buffer 0 (8 bytes of
// times, then 24 bytes of two VEC3 values).
std::string animatedNode(const std::string& input, const std::string& buffer)
{
  return R"({
"asset":{"version":"2.0"},
"scene":0,
"scenes":[{"nodes":[0]}],
"nodes":[{"name":"animated"}],
"buffers":[{"byteLength":32,"uri":"data:application/octet-stream;base64,)"
         + buffer + R"("}],
"bufferViews":[
 {"buffer":0,"byteOffset":0,"byteLength":8},
 {"buffer":0,"byteOffset":8,"byteLength":24}],
"accessors":[
 )" + input + R"(,
 {"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}],
"animations":[{"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}],
 "samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}]}]
})";
}

// Times (0, 1), values (0,0,0) (1,2,3).
constexpr const char* k_ascending = "AAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAQAAAQEA=";
// Times (1, 0), same values.
constexpr const char* k_descending = "AACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAAAQAAAQEA=";

constexpr const char* k_times_accessor
    = R"({"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[1]})";

std::unique_ptr<Threedim::GltfParser> load(const std::string& json)
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const std::string path = dir.filePath("anim.gltf").toStdString();
  {
    std::ofstream f(path, std::ios::binary);
    f.write(json.data(), std::streamsize(json.size()));
  }
  halp::text_file_view tv;
  tv.filename = path;
  auto apply = Threedim::GltfParser::ins::gltf_t::process(tv);
  if(!apply)
    return nullptr;
  auto parser = std::make_unique<Threedim::GltfParser>();
  apply(*parser);
  return parser;
}

std::size_t animationCount(const Threedim::GltfParser& p)
{
  const auto& st = p.m_raw_state;
  return st && st->animations ? st->animations->size() : 0;
}
}

TEST_CASE("a well-formed glTF animation is imported", "[threedim][gltf][animation]")
{
  auto p = load(animatedNode(k_times_accessor, k_ascending));
  REQUIRE(p);
  REQUIRE(animationCount(*p) == 1);
  const auto& anim = *(*p->m_raw_state->animations)[0];
  REQUIRE(anim.channels.size() == 1);
  CHECK(anim.duration == Catch::Approx(1.f));
  CHECK(*anim.channels[0].times == std::vector<float>{0.f, 1.f});
}

TEST_CASE("keyframe times that are not ascending drop the channel", "[threedim][gltf][animation]")
{
  auto p = load(animatedNode(k_times_accessor, k_descending));
  REQUIRE(p);
  CHECK(animationCount(*p) == 0);
}

TEST_CASE(
    "an accessor count backed by nothing in the file is refused before any "
    "allocation",
    "[threedim][gltf][animation]")
{
  // No bufferView: glTF reads such an accessor as zeros, so nothing bounds
  // `count` but the file itself. Two billion times would be an 8 GB vector.
  const std::string zeros
      = R"({"componentType":5126,"count":2000000000,"type":"SCALAR","min":[0],"max":[1]})";
  const auto start = std::chrono::steady_clock::now();
  auto p = load(animatedNode(zeros, k_ascending));
  const auto elapsed = std::chrono::steady_clock::now() - start;
  CHECK(!p);
  CHECK(elapsed < std::chrono::seconds(5));
}

namespace
{
constexpr uint64_t kNodeId = 1;

std::shared_ptr<ossia::scene_state> sceneWith(std::vector<float> times)
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

  ossia::animation_channel ch;
  ch.target_node_id = kNodeId;
  ch.target_path = ossia::animation_target::translation;
  ch.interpolation = ossia::animation_interpolation::linear;
  auto values = std::make_shared<std::vector<float>>();
  for(std::size_t k = 0; k < times.size(); k++)
    values->insert(values->end(), {float(k + 1), 0.f, 0.f});
  ch.values = std::move(values);
  ch.times = std::make_shared<std::vector<float>>(std::move(times));

  auto anim = std::make_shared<ossia::animation_component>();
  anim->duration = 2.f;
  anim->channels.push_back(std::move(ch));
  auto anims = std::make_shared<std::vector<ossia::animation_component_ptr>>();
  anims->push_back(anim);

  auto st = std::make_shared<ossia::scene_state>();
  st->roots = roots;
  st->animations = anims;
  st->version = 1;
  return st;
}

float translationXAt(const std::shared_ptr<ossia::scene_state>& scene, float time)
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
  REQUIRE(out->roots->size() == 1);
  const auto& node = (*out->roots)[0];
  REQUIRE(node->children);
  for(const auto& payload : *node->children)
    if(auto* xf = ossia::get_if<ossia::scene_transform>(&payload))
      return xf->translation[0];
  FAIL("no scene_transform on the animated node");
  return 0.f;
}
}

TEST_CASE("a NaN time samples the first key", "[threedim][animation]")
{
  const auto scene = sceneWith({0.f, 1.f, 2.f});
  CHECK(translationXAt(scene, 0.5f) == Catch::Approx(1.5f));
  CHECK(translationXAt(scene, std::numeric_limits<float>::quiet_NaN()) == 1.f);
}

TEST_CASE("a NaN last key clamps to that key", "[threedim][animation]")
{
  const auto scene = sceneWith({0.f, 1.f, std::numeric_limits<float>::quiet_NaN()});
  const float x = translationXAt(scene, 1.5f);
  CHECK(std::isfinite(x));
  CHECK(x == 3.f);
}

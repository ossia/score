// The audio protocol's parameters hold spans into the driver's buffers. When
// the channel count shrinks, or the engine goes away, the parameters that are
// left over must not keep pointing into buffers that no longer exist.

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/audio/audio_protocol.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <array>
#include <memory>
#include <vector>

#include <catch2/catch_all.hpp>

namespace
{
struct fixture
{
  static constexpr int frames = 32;

  ossia::audio_protocol* proto{new ossia::audio_protocol};
  ossia::net::generic_device dev{
      std::unique_ptr<ossia::net::protocol_base>(proto), "audio"};

  std::vector<std::array<float, frames>> storage;
  std::vector<float*> outs;

  void tick(int channels)
  {
    storage.assign(channels, {});
    outs.clear();
    for(auto& b : storage)
      outs.push_back(b.data());

    proto->setup_buffers(
        ossia::audio_tick_state{nullptr, outs.data(), 0, channels, frames, 0.});
  }

  ossia::audio_parameter* param(std::string_view path)
  {
    auto n = ossia::net::find_node(dev.get_root_node(), path);
    return n ? dynamic_cast<ossia::audio_parameter*>(n->get_parameter()) : nullptr;
  }
};
}

TEST_CASE("Shrinking the audio tree drops the leftover channel buffers", "[audio]")
{
  fixture f;
  f.proto->setup_tree(0, 8);
  f.tick(8);

  auto out8 = f.param("/out/8");
  REQUIRE(out8);
  REQUIRE(out8->audio.size() == 1);
  REQUIRE(!out8->audio[0].empty());

  f.proto->setup_tree(0, 2);

  CHECK(f.proto->audio_outs.size() == 2);
  REQUIRE(f.param("/out/8") == out8);
  CHECK(out8->audio[0].empty());
  CHECK(f.param("/out/main")->audio.size() == 2);
}

TEST_CASE("Stopping the audio protocol drops the driver buffers", "[audio]")
{
  fixture f;
  f.proto->setup_tree(0, 2);
  f.tick(2);

  REQUIRE(!f.param("/out/1")->audio[0].empty());
  REQUIRE(!f.param("/out/main")->audio[0].empty());

  f.proto->stop();

  CHECK(f.param("/out/1")->audio[0].empty());
  CHECK(f.param("/out/main")->audio[0].empty());
}

TEST_CASE("The audio tick never mutates the node tree", "[audio]")
{
  fixture f;
  f.proto->setup_tree(0, 2);

  // More channels than the tree knows about: the extra ones are ignored
  // rather than allocated for on the audio thread.
  f.tick(8);

  CHECK(f.proto->audio_outs.size() == 2);
  CHECK(f.param("/out/3") == nullptr);
  CHECK(!f.param("/out/2")->audio[0].empty());
}

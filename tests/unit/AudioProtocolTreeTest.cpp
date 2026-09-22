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

// ---------------------------------------------------------------------------
// What stop() does not promise
// ---------------------------------------------------------------------------

TEST_CASE("stop() is undone by the very next tick", "[audio]")
{
  fixture f;
  f.proto->setup_tree(0, 2);
  f.tick(2);

  f.proto->stop();
  CHECK(f.param("/out/1")->audio[0].empty());

  // Nothing in stop() prevents the protocol from being re-armed: any audio
  // callback that reaches setup_buffers rebinds every span. stop() is therefore
  // only meaningful once the engine is guaranteed parked.
  f.tick(2);
  CHECK(!f.param("/out/1")->audio[0].empty());
  CHECK(!f.param("/out/main")->audio[0].empty());
}

TEST_CASE("mapped parameters are not clamped by the tree", "[audio]")
{
  fixture f;
  f.proto->setup_tree(0, 2);

  auto& root = f.dev.get_root_node();
  auto& node = ossia::net::create_node(root, "custom");
  auto mapped = new ossia::mapped_audio_parameter{true, ossia::audio_mapping{5}, node};
  node.set_parameter(std::unique_ptr<ossia::net::parameter_base>(mapped));

  // The driver has 8 channels but the tree only knows 2.
  f.tick(8);

  // The physical /out/N parameters stop at 2...
  CHECK(f.proto->audio_outs.size() == 2);
  CHECK(f.param("/out/6") == nullptr);
  // ... but the mapping still reaches driver channel 5, because setup_buffers
  // clamps the mappings against state.n_out and not against the tree.
  REQUIRE(mapped->audio.size() == 1);
  CHECK(!mapped->audio[0].empty());
  CHECK(mapped->audio[0].data() == f.outs[5]);
}

TEST_CASE("A tick on a tree that was never built is silent, not a crash", "[audio]")
{
  fixture f;
  // setup_tree never called: this is what AudioDevice::reconnect() leaves
  // behind when there is no engine.
  REQUIRE(f.proto->main_audio_out == nullptr);

  f.tick(8);
  f.proto->advance_tick(1);

  CHECK(f.proto->audio_outs.empty());
  CHECK(f.param("/out/main") == nullptr);
}

TEST_CASE("Growing the tree rebinds every channel", "[audio]")
{
  fixture f;
  f.proto->setup_tree(0, 2);
  f.tick(2);
  auto out1 = f.param("/out/1");
  REQUIRE(!out1->audio[0].empty());

  f.proto->setup_tree(0, 8);
  // setup_tree unconditionally clears, including the channels it keeps.
  CHECK(out1->audio[0].empty());
  CHECK(f.param("/out/main")->audio.size() == 8);

  f.tick(8);
  CHECK(!out1->audio[0].empty());
  CHECK(!f.param("/out/8")->audio[0].empty());
}

TEST_CASE("Surplus nodes accept pushes without writing anywhere", "[audio]")
{
  fixture f;
  f.proto->setup_tree(0, 8);
  f.tick(8);
  f.proto->setup_tree(0, 2);
  f.tick(2);

  auto out8 = f.param("/out/8");
  REQUIRE(out8);
  REQUIRE(out8->audio.size() == 1);
  REQUIRE(out8->audio[0].empty());

  ossia::audio_port port;
  port.set_channels(1);
  port.channel(0).resize(fixture::frames);
  for(auto& v : port.channel(0))
    v = 1.f;

  // Must not write through the cleared span.
  out8->push_value(port);

  for(auto& b : f.storage)
    for(float v : b)
      CHECK(v == 0.f);
}

TEST_CASE("A shrink leaves no span into the previous driver's buffers", "[audio]")
{
  fixture f;
  f.proto->setup_tree(0, 8);
  {
    // A short-lived buffer set, as a driver that is about to go away.
    std::vector<std::array<float, fixture::frames>> old_storage(8);
    std::vector<float*> old_outs;
    for(auto& b : old_storage)
      old_outs.push_back(b.data());
    f.proto->setup_buffers(ossia::audio_tick_state{
        nullptr, old_outs.data(), 0, 8, fixture::frames, 0.});

    REQUIRE(!f.param("/out/8")->audio[0].empty());
    f.proto->setup_tree(0, 2);
  }
  // old_storage is gone; nothing may still point into it.
  for(auto p : {f.param("/out/1"), f.param("/out/2"), f.param("/out/8"),
                f.param("/out/main")})
  {
    REQUIRE(p);
    for(auto& chan : p->audio)
      CHECK(chan.empty());
  }
}

// Mirrors ApplicationPlugin::stop_engine(): proto->stop() runs *before*
// audio->stop(), so the engine is still live and still calling setup_buffers.
TEST_CASE("stop() before the engine is parked leaves dangling spans", "[audio]")
{
  fixture f;
  f.proto->setup_tree(0, 2);

  auto driver = std::make_unique<std::array<float, fixture::frames>[]>(2);
  std::vector<float*> outs{driver[0].data(), driver[1].data()};
  auto callback = [&] {
    f.proto->setup_buffers(
        ossia::audio_tick_state{nullptr, outs.data(), 0, 2, fixture::frames, 0.});
  };

  callback();
  REQUIRE(f.param("/out/1")->audio[0].data() == driver[0].data());

  // 1. stop_engine() clears the spans...
  f.proto->stop();
  CHECK(f.param("/out/1")->audio[0].empty());

  // 2. ... but the driver thread has not been stopped yet, so its next
  //    callback puts them straight back.
  callback();
  CHECK(f.param("/out/1")->audio[0].data() == driver[0].data());

  // 3. audio->stop() and the engine goes away, taking its buffers with it.
  auto stale = driver[0].data();
  driver.reset();

  // The protocol is left holding a span into freed memory - exactly what
  // stop() was added to prevent.
  CHECK(f.param("/out/1")->audio[0].data() == stale);
  CHECK(!f.param("/out/1")->audio[0].empty());
}

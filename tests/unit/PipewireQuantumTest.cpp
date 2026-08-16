// The PipeWire graph quantum is a global negotiation the client does not
// control: node.force-quantum is last-write-wins between clients, the global
// clock.force-quantum setting overrides all of them, and quantum-floor /
// quantum-limit clamp the result. These tests pin the pure cycle policy that
// lets the pipewire protocol process such cycles instead of skipping them
// (a skipped cycle leaves the filter's outputs in NEED_DATA — silence).

#include <ossia/audio/pipewire_quantum.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

using ossia::pipewire::quantum_tracker;
using event = quantum_tracker::event;

namespace
{
std::vector<std::pair<uint32_t, uint32_t>> chunks(uint32_t nframes, uint32_t max)
{
  std::vector<std::pair<uint32_t, uint32_t>> res;
  ossia::pipewire::for_each_chunk(
      nframes, max, [&](uint32_t offset, uint32_t n) { res.emplace_back(offset, n); });
  return res;
}
}

TEST_CASE("for_each_chunk: matching quantum is a single chunk", "[pipewire]")
{
  REQUIRE(chunks(128, 128) == std::vector<std::pair<uint32_t, uint32_t>>{{0, 128}});
}

TEST_CASE("for_each_chunk: larger quantum splits into block-sized slices", "[pipewire]")
{
  // The reported failure: graph at 512, engine configured for 128.
  REQUIRE(
      chunks(512, 128)
      == std::vector<std::pair<uint32_t, uint32_t>>{
          {0, 128}, {128, 128}, {256, 128}, {384, 128}});
}

TEST_CASE("for_each_chunk: non-multiple quantum ends with a short slice", "[pipewire]")
{
  REQUIRE(
      chunks(300, 128)
      == std::vector<std::pair<uint32_t, uint32_t>>{{0, 128}, {128, 128}, {256, 44}});
}

TEST_CASE("for_each_chunk: smaller quantum is a single short chunk", "[pipewire]")
{
  REQUIRE(chunks(96, 128) == std::vector<std::pair<uint32_t, uint32_t>>{{0, 96}});
}

TEST_CASE("for_each_chunk: degenerate sizes do nothing", "[pipewire]")
{
  REQUIRE(chunks(0, 128).empty());
  REQUIRE(chunks(512, 0).empty());
}

TEST_CASE("quantum_tracker: logs once per reconfiguration, not per cycle", "[pipewire]")
{
  quantum_tracker t{.expected = 128};

  // Startup at the requested quantum: nothing to say.
  REQUIRE(t.observe(128) == event::matched);
  REQUIRE(t.observe(128) == event::steady);

  // Another client flips the graph to 512: one warning, then quiet.
  REQUIRE(t.observe(512) == event::mismatch);
  REQUIRE(t.observe(512) == event::steady);
  REQUIRE(t.observe(512) == event::steady);

  // Flips again: one warning for the new value.
  REQUIRE(t.observe(1024) == event::mismatch);
  REQUIRE(t.observe(1024) == event::steady);

  // Back to what we asked for: one info.
  REQUIRE(t.observe(128) == event::recovered);
  REQUIRE(t.observe(128) == event::steady);
}

TEST_CASE("quantum_tracker: startup mismatch warns immediately", "[pipewire]")
{
  // First cycles arrive before the driver picks up our forced quantum.
  quantum_tracker t{.expected = 128};
  REQUIRE(t.observe(1024) == event::mismatch);
  REQUIRE(t.observe(1024) == event::steady);
  REQUIRE(t.observe(128) == event::recovered);
}

TEST_CASE("assign_chunk_pointers: offsets live channels, scratch for dead ones", "[pipewire]")
{
  std::array<float, 512> ch0{}, ch2{};
  std::array<float, 128> scratch{};
  std::array<float*, 3> cycle{ch0.data(), nullptr, ch2.data()};
  std::array<float*, 3> chunk{};

  ossia::pipewire::assign_chunk_pointers(
      cycle.data(), chunk.data(), cycle.size(), 256, scratch.data());

  REQUIRE(chunk[0] == ch0.data() + 256);
  REQUIRE(chunk[1] == scratch.data());
  REQUIRE(chunk[2] == ch2.data() + 256);
}

// Monitoring rings: what the execution thread hands to the views.
// SampleRing returns only one continuous stream, EventRing keeps events
// while they fit and counts the rest.

#include "AllocCounter.hpp"

#include <Process/Monitoring/EventRing.hpp>
#include <Process/Monitoring/Monitor.hpp>
#include <Process/Monitoring/SampleRing.hpp>

#include <catch2/catch_test_macros.hpp>

#include <thread>
#include <vector>

namespace
{
void writeConstant(Process::SampleRing& ring, int channels, int frames, float v)
{
  std::vector<float> data(frames, v);
  ring.write(channels, frames, [&](int) { return data.data(); });
}

}

TEST_CASE("SampleRing: a channel provisioned mid-stream has no past", "[monitoring]")
{
  Process::SampleRing ring{4096};
  ring.set_rate(48000.);
  writeConstant(ring, 1, 64, 1.f);
  REQUIRE(ring.channels() == 1);

  // No buffer yet: nothing to read
  std::vector<float> dst(32);
  REQUIRE_FALSE(ring.read(0, 0, dst));

  ring.provision();
  // The zeros of the new buffer are not samples of the signal
  REQUIRE_FALSE(ring.read(0, 32, dst));

  writeConstant(ring, 1, 64, 2.f);
  REQUIRE(ring.read(0, 64, dst));
  for(float v : dst)
    REQUIRE(v == 2.f);
  // Straddling the first block of the buffer
  REQUIRE_FALSE(ring.read(0, 48, dst));
}

TEST_CASE("SampleRing: a skipped channel starts again", "[monitoring]")
{
  Process::SampleRing ring{4096};
  writeConstant(ring, 2, 64, 1.f);
  ring.provision();
  writeConstant(ring, 2, 64, 1.f);
  std::vector<float> dst(32);
  REQUIRE(ring.read(1, 64, dst));

  writeConstant(ring, 1, 64, 1.f);
  writeConstant(ring, 2, 64, 3.f);
  REQUIRE_FALSE(ring.read(1, 128, dst));
  REQUIRE(ring.read(1, 192, dst));
  REQUIRE(dst.front() == 3.f);
  // Channel 0 was written all along
  REQUIRE(ring.read(0, 128, dst));
}

TEST_CASE("SampleRing: overwritten frames are not returned", "[monitoring]")
{
  Process::SampleRing ring{1024};
  writeConstant(ring, 1, 64, 0.f);
  ring.provision();
  for(int i = 0; i < 40; i++)
    writeConstant(ring, 1, 64, float(i));

  std::vector<float> dst(64);
  const uint64_t w = ring.written();
  REQUIRE(ring.read(0, w - 64, dst));
  REQUIRE(dst.front() == 39.f);
  // More than a capacity ago
  REQUIRE_FALSE(ring.read(0, w - 1024 - 64, dst));
  uint64_t first{};
  REQUIRE(ring.read_latest(0, dst, first));
  REQUIRE(first == w - 64);
}

TEST_CASE("SampleRing: a pause of the writer cuts the stream", "[monitoring]")
{
  Process::SampleRing ring{4096};
  ring.set_rate(48000.);
  writeConstant(ring, 1, 64, 0.f);
  ring.provision();
  for(int i = 0; i < 8; i++)
    writeConstant(ring, 1, 64, 1.f);

  std::vector<float> dst(256);
  uint64_t first{};
  REQUIRE(ring.read_latest(0, dst, first));

  // Not watched for a while, then watched again
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  writeConstant(ring, 1, 64, 2.f);
  REQUIRE_FALSE(ring.read_latest(0, dst, first));
  for(int i = 0; i < 3; i++)
    writeConstant(ring, 1, 64, 2.f);
  REQUIRE(ring.read_latest(0, dst, first));
  for(float v : dst)
    REQUIRE(v == 2.f);

  ring.restart();
  writeConstant(ring, 1, 64, 3.f);
  REQUIRE_FALSE(ring.read_latest(0, dst, first));
}

TEST_CASE("SampleRing: the writer does not allocate", "[monitoring]")
{
  Process::SampleRing ring{4096};
  writeConstant(ring, 2, 64, 0.f);
  ring.provision();
  std::vector<float> data(64, 1.f);
  alloc_counter::scope scope;
  for(int i = 0; i < 100; i++)
    ring.write(2, 64, [&](int) { return data.data(); });
  REQUIRE(scope.allocations() == 0);
}

TEST_CASE("EventRing: events in order, the overflow counted", "[monitoring]")
{
  Process::EventRing<int, 16> ring;
  int pushed = 0;
  {
    alloc_counter::scope scope;
    while(ring.push(pushed))
      pushed++;
    REQUIRE(scope.allocations() == 0);
  }
  REQUIRE(pushed >= 16);
  REQUIRE(ring.dropped() == 1);

  // Outside of a refresh of the views, a drain takes the events
  REQUIRE(Process::monitorFrame() == 0);
  std::vector<int> got;
  REQUIRE(ring.drain([&](int e) { got.push_back(e); }) == pushed);
  REQUIRE(int(got.size()) == pushed);
  for(int i = 0; i < pushed; i++)
    REQUIRE(got[i] == i);
  REQUIRE(ring.drain([&](int) {}) == 0);

  REQUIRE(ring.push(42));
  got.clear();
  ring.drain([&](int e) { got.push_back(e); });
  REQUIRE(got == std::vector<int>{42});
}

TEST_CASE("LatestValue: the newest frame, once", "[monitoring]")
{
  Process::LatestValue<int> v;
  int out = -1;
  REQUIRE_FALSE(v.consume(out));
  v.produce(1);
  v.produce(2);
  REQUIRE(v.consume(out));
  REQUIRE(out == 2);
  REQUIRE_FALSE(v.consume(out));
}

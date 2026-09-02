// The GStreamer audio ring buffer and its output-span invariant (680dccbd29,
// part c).
//
// read_into_output() runs on the audio thread and resizes the parameter's
// output vectors to the engine tick. The spans the engine reads through are
// stored separately, so a resize that changed a vector's data pointer without
// re-pointing them would leave the engine reading freed memory.

#include <Gfx/GStreamer/GStreamerAudioBuffer.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace Gfx::GStreamer;

namespace
{
struct rig
{
  AudioBuffer buf;
  std::vector<ossia::float_vector> data;
  ossia::small_vector<std::span<float>, 8> spans;

  explicit rig(int channels, int block)
  {
    buf.init(channels);
    data.resize(channels);
    spans.resize(channels);
    for(int i = 0; i < channels; i++)
    {
      data[i].reserve(AudioBuffer::max_block);
      data[i].resize(block, 0.f);
      spans[i] = data[i];
    }
    buf.output_data = &data;
    buf.output_spans = &spans;
  }

  void writeRamp(int samples, int channels, float base = 0.f)
  {
    std::vector<float> interleaved(std::size_t(samples) * channels);
    for(int s = 0; s < samples; s++)
      for(int c = 0; c < channels; c++)
        interleaved[std::size_t(s) * channels + c] = base + float(s) + float(c) * 0.5f;
    buf.write(interleaved.data(), samples, channels);
  }
};
}

TEST_CASE("a resize re-points the output spans", "[unit][gstreamer][audio]")
{
  rig r{2, 64};
  const float* before[2] = {r.data[0].data(), r.data[1].data()};

  r.writeRamp(256, 2);
  r.buf.read_into_output(128);

  for(int i = 0; i < 2; i++)
  {
    INFO("channel " << i);
    CHECK(r.data[i].size() == 128u);
    // The capacity was reserved up front, so growing must not have moved the
    // storage the spans already pointed at.
    CHECK(r.data[i].data() == before[i]);
    CHECK(r.spans[i].data() == r.data[i].data());
    CHECK(r.spans[i].size() == 128u);
  }
}

TEST_CASE("the block size is clamped to max_block", "[unit][gstreamer][audio]")
{
  rig r{2, 64};
  const float* before = r.data[0].data();

  r.buf.read_into_output(1 << 20);

  CHECK(r.data[0].size() == AudioBuffer::max_block);
  CHECK(r.data[0].data() == before);
  CHECK(r.spans[0].size() == AudioBuffer::max_block);
  CHECK(r.spans[0].data() == r.data[0].data());
}

TEST_CASE("shrinking re-points the spans too", "[unit][gstreamer][audio]")
{
  rig r{2, 64};
  r.buf.read_into_output(512);
  CHECK(r.spans[0].size() == 512u);

  r.buf.read_into_output(32);
  CHECK(r.data[0].size() == 32u);
  CHECK(r.spans[0].size() == 32u);
  CHECK(r.spans[0].data() == r.data[0].data());
}

TEST_CASE("a torn-down parameter makes the read a no-op", "[unit][gstreamer][audio]")
{
  rig r{2, 64};
  r.buf.output_data = nullptr;
  r.buf.output_spans = nullptr;
  r.buf.read_into_output(128);
  CHECK(r.data[0].size() == 64u);

  // Half a teardown: the storage still follows the tick, and nothing
  // dereferences the null span list.
  r.buf.output_data = &r.data;
  r.buf.read_into_output(128);
  CHECK(r.data[0].size() == 128u);
  CHECK(r.spans[0].size() == 64u);
}

TEST_CASE("samples come out in the order they went in", "[unit][gstreamer][audio]")
{
  rig r{2, 64};
  r.writeRamp(64, 2);
  r.buf.read_into_output(64);

  for(int s = 0; s < 64; s++)
  {
    CHECK(r.data[0][s] == float(s));
    CHECK(r.data[1][s] == float(s) + 0.5f);
  }
}

TEST_CASE("an underrun yields silence rather than stale samples", "[unit][gstreamer][audio]")
{
  rig r{2, 64};
  r.writeRamp(64, 2, /*base=*/1.f);
  r.buf.read_into_output(64);
  REQUIRE(r.data[0][0] == 1.f);

  // Nothing more was written: the next tick must not repeat the last block.
  r.buf.read_into_output(64);
  for(int s = 0; s < 64; s++)
    CHECK(r.data[0][s] == 0.f);
}

TEST_CASE("the ring keeps its ordering across a wrap", "[unit][gstreamer][audio]")
{
  rig r{1, 1024};
  const int block = 1024;
  const int blocks = int(AudioBuffer::ring_size / block) + 3;

  for(int b = 0; b < blocks; b++)
  {
    r.writeRamp(block, 1, /*base=*/float(b) * block);
    r.buf.read_into_output(block);
    INFO("block " << b << " of " << blocks);
    REQUIRE(r.data[0].size() == std::size_t(block));
    CHECK(r.data[0][0] == float(b) * block);
    CHECK(r.data[0][block - 1] == float(b) * block + float(block - 1));
  }
}

TEST_CASE("a wider stream than the buffer has channels is truncated", "[unit][gstreamer][audio]")
{
  rig r{2, 64};
  r.writeRamp(64, 4);
  r.buf.read_into_output(64);
  CHECK(r.data[0][3] == 3.f);
  CHECK(r.data[1][3] == 3.5f);
}

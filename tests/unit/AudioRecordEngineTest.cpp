// The audio recorder of the recording engine, driven tick by tick as the
// graph drives it: its clock is the node of a box's interval, whose requested
// tokens say which dates of the box each buffer covers.
//
// The input carries, on every sample, the index of that sample since the
// start (plus the channel), so what was recorded says exactly which samples
// it holds:
//  - started at once, it records every sample from the tick it starts in;
//  - before its box plays, nothing;
//  - started on the next bar, its first sample is the bar's;
//  - with a length, it stops on the sample the length ends at;
//  - with its blocks all in use, it counts what it could not record.

#include <Recording/Engine/AudioRecorder.hpp>

#include <ossia/dataflow/audio_port.hpp>
#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/nodes/forward_node.hpp>

#include <ossia/dataflow/nodes/sound_utils.hpp>

#include <span>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using namespace Recording;

namespace
{
constexpr int rate = 48000;
constexpr int buffer = 480; // 10 ms
constexpr int64_t flicks_per_second = 705600000;
constexpr int64_t tick_flicks = flicks_per_second / 100;
constexpr double Q = 352800000.0; // flicks per quarter at 120 bpm

float value(int64_t sample, int channel)
{
  return float(sample * 1e-6 + channel);
}

struct Rig
{
  ossia::execution_state st;
  std::shared_ptr<ossia::nodes::forward_node> clock
      = std::make_shared<ossia::nodes::forward_node>();
  std::shared_ptr<AudioTake> take;
  std::unique_ptr<audio_record_node> node;
  int64_t sample = 0;   // since the start, in the input
  int64_t boxDate = 0;  // where the box is, in flicks
  std::vector<std::vector<float>> recorded;
  //! Channels the source gives
  int channels = 2;
  //! Frames the last channel has in a buffer; -1: all of them
  int lastChannelFrames = -1;

  explicit Rig(int blocks = 64)
  {
    st.sampleRate = rate;
    st.bufferSize = buffer;
    st.modelToSamplesRatio = double(rate) / flicks_per_second;
    st.samplesToModelRatio = flicks_per_second / double(rate);
    take = std::make_shared<AudioTake>(2, 512, blocks);
    node = std::make_unique<audio_record_node>(take, clock);
    recorded.resize(2);
  }

  //! One buffer; the box plays during it when `plays`
  void tick(bool plays)
  {
    auto& in = *node->root_inputs()[0]->target<ossia::audio_port>();
    in.set_channels(channels);
    for(int c = 0; c < channels; c++)
    {
      const int frames
          = (c == channels - 1 && lastChannelFrames >= 0) ? lastChannelFrames : buffer;
      in.channel(c).resize(frames);
      for(int k = 0; k < frames; k++)
        in.channel(c)[k] = value(sample + k, c);
    }

    clock->requested_tokens.clear();
    if(plays)
    {
      ossia::token_request tk;
      tk.prev_date = ossia::time_value{boxDate};
      tk.date = ossia::time_value{boxDate + tick_flicks};
      tk.speed = 1.;
      tk.tempo = 120.;
      tk.signature = ossia::time_signature{4, 4};
      tk.start_sample = 0;
      tk.length_sample = buffer;
      tk.musical_start_position = boxDate / Q;
      tk.musical_end_position = (boxDate + tick_flicks) / Q;
      tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
      tk.musical_end_last_bar = std::floor(tk.musical_end_position / 4.) * 4.;
      clock->requested_tokens.push_back(tk);
      boxDate += tick_flicks;
    }

    node->run(ossia::token_request{}, ossia::exec_state_facade{&st});
    sample += buffer;
  }

  //! What the writer thread would do
  void drain()
  {
    take->drain([&](AudioTake::Block& b) {
      if(int(recorded.size()) < b.channels)
        recorded.resize(b.channels);
      for(int c = 0; c < b.channels; c++)
      {
        const float* p = b.channel(c);
        recorded[c].insert(recorded[c].end(), p, p + b.frames);
      }
    });
  }

  //! The input sample each recorded frame of channel 0 came from
  int64_t sampleOf(std::size_t frame) const
  {
    return std::llround(recorded[0][frame] * 1e6);
  }
};
}

TEST_CASE("Started at once, the recorder records every sample", "[recording][audio]")
{
  Rig r;
  r.take->startRequested = true;
  for(int i = 0; i < 10; i++)
  {
    r.tick(true);
    r.drain();
  }
  r.take->stopRequested = true;
  r.tick(true);
  r.drain();

  CHECK(r.take->state == AudioTake::Done);
  CHECK(r.take->startDate == 0);
  CHECK(r.take->endDate == 10 * tick_flicks);
  CHECK(r.take->frames == 10 * buffer);
  REQUIRE(r.recorded[0].size() == std::size_t(10 * buffer));
  int wrong = 0;
  for(std::size_t i = 0; i < r.recorded[0].size(); i++)
  {
    wrong += r.sampleOf(i) != int64_t(i);
    wrong += r.recorded[1][i] != value(int64_t(i), 1);
  }
  CHECK(wrong == 0);
  CHECK(r.take->droppedFrames == 0);
}

TEST_CASE("The recorder waits for its box to play", "[recording][audio]")
{
  Rig r;
  r.take->startRequested = true;
  for(int i = 0; i < 5; i++)
    r.tick(false);
  CHECK(r.take->state == AudioTake::Waiting);
  CHECK(r.take->frames == 0);
  for(int i = 0; i < 3; i++)
    r.tick(true);
  r.take->stopRequested = true;
  r.tick(true);
  r.drain();
  REQUIRE(r.recorded[0].size() == std::size_t(3 * buffer));
  CHECK(r.sampleOf(0) == 5 * buffer);
  CHECK(r.take->startDate == 0);
}

TEST_CASE("Started on the next bar, the take begins on the bar's sample", "[recording][audio]")
{
  Rig r;
  // Half a second into the box: the next bar is at 2 s, sample 96000
  for(int i = 0; i < 50; i++)
    r.tick(true);
  r.take->startRate = 1.;
  r.take->startRequested = true;
  for(int i = 0; i < 200; i++)
  {
    r.tick(true);
    r.drain();
  }
  CHECK(r.take->state == AudioTake::Recording);
  // Stopped: the block being filled goes to the writer too
  r.take->stopRequested = true;
  r.tick(true);
  r.drain();
  CHECK(r.take->state == AudioTake::Done);
  CHECK(r.take->startPosition == Catch::Approx(4.));
  CHECK(std::abs(r.take->startDate - int64_t(4 * Q)) <= 1);
  REQUIRE(!r.recorded[0].empty());
  CHECK(r.sampleOf(0) == 2 * rate);
  CHECK(r.sampleOf(r.recorded[0].size() - 1) == 250 * buffer - 1);
}

TEST_CASE("A take with a length stops on the sample it ends at", "[recording][audio]")
{
  Rig r;
  // Started 0.25 s in, one bar long: 2 s, 96000 samples
  for(int i = 0; i < 25; i++)
    r.tick(true);
  r.take->length = int64_t(4 * Q);
  r.take->startRequested = true;
  for(int i = 0; i < 300; i++)
  {
    r.tick(true);
    r.drain();
  }
  CHECK(r.take->state == AudioTake::Done);
  CHECK(r.take->frames == 2 * rate);
  REQUIRE(r.recorded[0].size() == std::size_t(2 * rate));
  CHECK(r.sampleOf(0) == 25 * buffer);
  CHECK(r.sampleOf(r.recorded[0].size() - 1) == 25 * buffer + 2 * rate - 1);
  CHECK(r.take->endDate == r.take->startDate + int64_t(4 * Q));
}

TEST_CASE("A recorder whose blocks are all in use counts what it loses", "[recording][audio]")
{
  Rig r{2}; // two blocks of 512 frames, never given back
  r.take->startRequested = true;
  for(int i = 0; i < 10; i++)
    r.tick(true);
  CHECK(r.take->frames == 2 * 512);
  CHECK(r.take->frames + r.take->droppedFrames == 10 * buffer);
}

TEST_CASE("A source of more channels than a block was sized for is recorded whole, in smaller blocks", "[recording][audio][channels]")
{
  Rig r;
  r.channels = 16;
  r.take->startRequested = true;
  for(int i = 0; i < 40; i++)
  {
    r.tick(true);
    r.drain();
  }
  r.take->stopRequested = true;
  r.tick(true);
  r.drain();
  CHECK(r.take->activeChannels == 16);
  CHECK(r.take->lostChannels == 0);
  CHECK(r.take->droppedFrames == 0);
  REQUIRE(r.recorded.size() == 16);
  const std::size_t frames = r.recorded[0].size();
  REQUIRE(frames >= std::size_t(40 * buffer));
  for(int c = 0; c < 16; c++)
  {
    REQUIRE(r.recorded[c].size() == frames);
    int64_t wrong = 0;
    for(std::size_t k = 0; k < frames; k++)
      wrong += std::abs(r.recorded[c][k] - value(int64_t(k), c)) > 1e-4;
    INFO("channel " << c);
    CHECK(wrong == 0);
  }
}

TEST_CASE("Channels beyond the most a take keeps are counted, the others recorded", "[recording][audio][channels]")
{
  Rig r;
  r.channels = AudioTake::maxChannels + 6;
  r.take->startRequested = true;
  for(int i = 0; i < 4; i++)
  {
    r.tick(true);
    r.drain();
  }
  r.take->stopRequested = true;
  r.tick(true);
  r.drain();
  CHECK(r.take->activeChannels == AudioTake::maxChannels);
  CHECK(r.take->lostChannels == 6);
  REQUIRE(int(r.recorded.size()) == AudioTake::maxChannels);
  CHECK(r.recorded[0].size() >= std::size_t(4 * buffer));
  CHECK(std::abs(r.recorded[AudioTake::maxChannels - 1][10] - value(10, AudioTake::maxChannels - 1)) < 1e-4);
}

TEST_CASE("A channel shorter than the buffer is recorded with silence after it", "[recording][audio][channels]")
{
  Rig r;
  r.lastChannelFrames = buffer / 2;
  r.take->startRequested = true;
  for(int i = 0; i < 4; i++)
  {
    r.tick(true);
    r.drain();
  }
  r.take->stopRequested = true;
  r.tick(true);
  r.drain();
  REQUIRE(r.recorded.size() == 2);
  REQUIRE(r.recorded[1].size() >= std::size_t(4 * buffer));
  int64_t wrong = 0;
  for(int k = 0; k < 4 * buffer; k++)
  {
    const float expected = (k % buffer) < buffer / 2 ? value(k, 1) : 0.f;
    wrong += std::abs(r.recorded[1][k] - expected) > 1e-4;
  }
  CHECK(wrong == 0);
  // The whole channel beside it is untouched
  for(int k = 0; k < 4 * buffer; k++)
    wrong += std::abs(r.recorded[0][k] - value(k, 0)) > 1e-4;
  CHECK(wrong == 0);
}

// A sound is set to loop and given its loop length by two commands, which a
// tick can fall between: looping without a length yet reads the plain sound.
TEST_CASE("A sound looping without a loop length yet is read as the plain sound", "[sound][regression]")
{
  constexpr int64_t len = 64;
  std::vector<float> left(len);
  for(int64_t i = 0; i < len; i++)
    left[i] = 1.f + i;
  ossia::audio_span<float> data;
  data.push_back(std::span<const float>(left.data(), len));

  constexpr int64_t n = 16;
  for(int64_t start : {int64_t(0), int64_t(8), len - 4})
  {
    std::vector<float> out_l(n, -1.f);
    float* out[1] = {out_l.data()};
    ossia::read_audio_from_buffer<float>(data, start, n, 0, 0, true, out);
    for(int64_t k = 0; k < n; k++)
    {
      const int64_t pos = start + k;
      CHECK(out_l[k] == (pos < len ? left[pos] : 0.f));
    }
  }
}

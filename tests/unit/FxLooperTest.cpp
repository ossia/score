#include <Fx/Looper.hpp>

#include <ossia/dataflow/execution_state.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

using Catch::Approx;

namespace
{
using Looper = Nodes::AudioLooper::Node;
using LoopMode = Looper::LoopMode;
using Postaction = Looper::Postaction;

// Drives a Looper the way avnd's ossia binding does: the sample pointers handed
// to the node are already offset by the tick's start sample, and the node is
// expected to write [0; length[ from there. The storage is always max_channels
// wide while the busses only advertise `channels`, so a write past the last
// advertised channel lands on a buffer we can check instead of on the heap.
struct looper_harness
{
  static constexpr int max_channels = 4;
  static constexpr double sentinel = -1234.5;

  Looper node;
  ossia::execution_state st;

  std::array<std::vector<double>, max_channels> in_buf, out_buf;
  std::array<double*, max_channels> in_ptr{}, out_ptr{};
  int channels{1};
  int buffer_size{64};
  int64_t now{0};

  explicit looper_harness(int chans = 1, int bs = 64, int rate = 48000)
      : channels{chans}
      , buffer_size{bs}
  {
    st.sampleRate = rate;
    st.bufferSize = bs;
    st.modelToSamplesRatio = 1.;
    st.samplesToModelRatio = 1.;
    node.ossia_state = {&st};

    // Quantification is a quarter note by default; most cases here are about
    // the sample machinery, not the musical grid.
    node.inputs.quantif.value = 0.f;

    for(int i = 0; i < max_channels; i++)
    {
      in_buf[i].assign(bs, 0.);
      out_buf[i].assign(bs, sentinel);
    }
  }

  void fill_input(int chan, double (*f)(int64_t))
  {
    for(int64_t i = 0; i < buffer_size; i++)
      in_buf[chan][i] = f(i);
  }

  void reset_output() { for(auto& c : out_buf) std::fill(c.begin(), c.end(), sentinel); }
  void reset_input() { for(auto& c : in_buf) std::fill(c.begin(), c.end(), 0.); }

  ossia::token_request tick(int64_t frames, int64_t start = 0)
  {
    ossia::token_request tk;
    tk.prev_date = ossia::time_value{now};
    tk.date = ossia::time_value{now + frames};
    tk.start_sample = start;
    tk.length_sample = frames;
    tk.tempo = 120.;
    now += frames;
    return tk;
  }

  void run(const ossia::token_request& tk)
  {
    const auto start = node.ossia_state.timings(tk).start_sample;
    for(int i = 0; i < max_channels; i++)
    {
      in_ptr[i] = in_buf[i].data() + start;
      out_ptr[i] = out_buf[i].data() + start;
    }
    node.inputs.audio.samples = in_ptr.data();
    node.inputs.audio.channels = channels;
    node.outputs.audio.samples = out_ptr.data();
    node.outputs.audio.channels = channels;
    node(tk);
  }

  void run(int64_t frames, int64_t start = 0) { run(tick(frames, start)); }

  const ossia::audio_channel& loop(int chan = 0) const { return node.state.audio[chan]; }
};
}

TEST_CASE("Looper: stopped, the input passes through untouched", "[fx][audio][looper]")
{
  looper_harness h;
  h.node.inputs.mode = LoopMode::Stop;
  h.fill_input(0, [](int64_t i) { return double(i + 1); });

  h.run(64);

  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(double(i + 1)));
}

TEST_CASE("Looper: record then play repeats the recorded material", "[fx][audio][looper]")
{
  looper_harness h;
  h.fill_input(0, [](int64_t i) { return double(i + 1); });

  h.node.inputs.mode = LoopMode::Record;
  h.run(64);
  REQUIRE(h.loop().size() == 64);

  h.node.inputs.mode = LoopMode::Play;
  h.reset_output();
  h.run(64);
  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(h.loop()[i]));

  h.reset_output();
  h.run(64);
  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(h.loop()[i]));
}

// A tick that does not begin at sample 0 of the buffer - a scenario cut, or the
// second half of a quantized mode change - must write exactly its own span.
TEST_CASE("Looper: a tick starting mid-buffer writes its own span", "[fx][audio][looper]")
{
  looper_harness h;
  h.node.inputs.mode = LoopMode::Stop;
  h.fill_input(0, [](int64_t i) { return double(i + 1); });

  h.run(32, 32);

  for(int i = 0; i < 32; i++)
    CHECK(h.out_buf[0][i] == Approx(looper_harness::sentinel));
  for(int i = 32; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(double(i + 1)));
}

TEST_CASE("Looper: a loop shorter than one buffer fills the whole output",
          "[fx][audio][looper]")
{
  looper_harness h;
  h.fill_input(0, [](int64_t i) { return double(i + 1); });

  h.node.inputs.mode = LoopMode::Record;
  h.run(24);
  REQUIRE(h.loop().size() == 24);

  h.node.inputs.mode = LoopMode::Play;
  h.reset_output();
  h.run(64);

  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(h.loop()[i % 24]));
}

TEST_CASE("Looper: playback wraps on the loop length, not on the buffer length",
          "[fx][audio][looper]")
{
  constexpr int64_t loop_len = 100;
  looper_harness h{1, 128};
  h.fill_input(0, [](int64_t i) { return double(i + 1); });

  h.node.inputs.mode = LoopMode::Record;
  h.run(loop_len);
  REQUIRE(h.loop().size() == loop_len);

  h.node.inputs.mode = LoopMode::Play;
  int64_t pos = 0;
  for(int t = 0; t < 4; t++)
  {
    h.reset_output();
    h.run(128);
    for(int i = 0; i < 128; i++)
      CHECK(h.out_buf[0][i] == Approx(h.loop()[(pos + i) % loop_len]));
    pos = (pos + 128) % loop_len;
  }
}

TEST_CASE("Looper: overdubbing an empty loop writes nothing into it",
          "[fx][audio][looper]")
{
  looper_harness h;
  // The constructor reserves a large capacity, which is what hides an
  // out-of-bounds write on a loop of length zero.
  h.node.state.audio[0].shrink_to_fit();
  h.node.state.audio[1].shrink_to_fit();

  h.fill_input(0, [](int64_t i) { return double(i + 1); });
  h.node.inputs.mode = LoopMode::Overdub;
  h.run(64);

  CHECK(h.loop().size() == 0);
  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(double(i + 1)));
}

TEST_CASE("Looper: overdub keeps its phase across a loop that is not a multiple "
          "of the buffer",
          "[fx][audio][looper]")
{
  constexpr int64_t loop_len = 100;
  looper_harness h;

  h.node.inputs.mode = LoopMode::Record;
  h.run(64);
  h.run(36);
  REQUIRE(h.loop().size() == loop_len);

  h.node.inputs.mode = LoopMode::Overdub;
  h.run(64); // 0 -> 64
  h.run(64); // 64 -> 99, 0 -> 27

  // One impulse on the first sample of the third overdub pass.
  h.in_buf[0][0] = 1.;
  h.run(64);

  CHECK(h.loop()[28] == Approx(1.));
  CHECK(h.loop()[0] == Approx(0.));
}

TEST_CASE("Looper: playback never writes past the advertised output channels",
          "[fx][audio][looper]")
{
  looper_harness h{2};
  h.fill_input(0, [](int64_t i) { return double(i + 1); });
  h.fill_input(1, [](int64_t i) { return double(i + 1); });

  h.node.inputs.mode = LoopMode::Record;
  h.run(64);
  REQUIRE(h.node.state.channels() == 2);

  // The source is re-patched to a mono one: the output bus now mimics one
  // channel, the recorded loop still has two.
  h.channels = 1;
  h.node.inputs.mode = LoopMode::Play;
  h.reset_output();
  h.run(64);

  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[1][i] == Approx(looper_harness::sentinel));
}

TEST_CASE("Looper: a channel that comes back is as long as the ones that stayed",
          "[fx][audio][looper]")
{
  looper_harness h{2};
  h.fill_input(0, [](int64_t i) { return double(i + 1); });
  h.fill_input(1, [](int64_t i) { return double(i + 1); });

  h.node.inputs.mode = LoopMode::Record;
  h.run(64);

  // The source narrows to mono, then widens back.
  h.channels = 1;
  h.run(64);
  h.channels = 2;

  h.node.inputs.mode = LoopMode::Overdub;
  h.run(64);

  REQUIRE(h.node.state.channels() == 2);
  CHECK(h.loop(1).size() == h.loop(0).size());
}

TEST_CASE("Looper: the loop is faded once, not on every mode change",
          "[fx][audio][looper]")
{
  looper_harness h{1, 256};
  h.fill_input(0, [](int64_t) { return 1.; });

  h.node.inputs.mode = LoopMode::Record;
  h.run(200);
  REQUIRE(h.loop().size() == 200);

  h.node.inputs.mode = LoopMode::Play;
  h.run(200);
  h.node.inputs.mode = LoopMode::Stop;
  h.run(200);
  h.node.inputs.mode = LoopMode::Play;
  h.run(200);

  // Index 64 sits in the 128-sample fade-in and nowhere else.
  CHECK(h.loop()[64] == Approx(0.5));
}

TEST_CASE("Looper: a post-action of zero bars does not cancel the recording",
          "[fx][audio][looper]")
{
  looper_harness h{1, 128};
  h.node.inputs.quantif.value = 0.25f;
  h.node.inputs.postaction_bars.value = 0;
  h.fill_input(0, [](int64_t i) { return double(i + 1); });

  h.node.inputs.mode = LoopMode::Record;
  h.run(128);
  h.run(128);

  CHECK(h.loop().size() == 256);
}

TEST_CASE("Looper: recording without passthrough emits silence, not the previous tick",
          "[fx][audio][looper]")
{
  looper_harness h;
  h.node.inputs.passthrough.value = false;
  h.fill_input(0, [](int64_t i) { return double(i + 1); });

  h.node.inputs.mode = LoopMode::Record;
  h.run(64);

  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(0.));
}

TEST_CASE("Looper: the first buffer, with nothing recorded, is a passthrough",
          "[fx][audio][looper]")
{
  looper_harness h;
  h.node.inputs.quantif.value = 0.25f; // the shipped default
  h.fill_input(0, [](int64_t i) { return double(i + 1); });

  h.run(64);

  CHECK(h.loop().size() == 0);
  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(double(i + 1)));
}

TEST_CASE("Looper: a post-action bar line inside a tick covers both halves",
          "[fx][audio][looper]")
{
  looper_harness h;
  h.node.inputs.postaction_bars.value = 1;
  h.node.inputs.postaction = Postaction::Play;
  h.fill_input(0, [](int64_t i) { return double(i + 1); });

  h.node.inputs.mode = LoopMode::Record;
  {
    auto tk = h.tick(64);
    tk.musical_start_position = 0.;
    tk.musical_end_position = 1.;
    h.run(tk);
  }
  REQUIRE(h.loop().size() == 64);

  // The recording runs out one bar in, halfway through this tick.
  h.reset_output();
  {
    auto tk = h.tick(64);
    tk.musical_start_position = 3.5;
    tk.musical_end_position = 4.5;
    tk.musical_start_last_bar = 0.;
    tk.musical_end_last_bar = 4.;
    h.run(tk);
  }

  CHECK(h.loop().size() == 96);
  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] != Approx(looper_harness::sentinel));
  for(int i = 32; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(h.loop()[i - 32]));
  CHECK(h.node.state.playbackPos == 32);
}

TEST_CASE("Looper: a quantized mode change covers both halves of the tick",
          "[fx][audio][looper]")
{
  looper_harness h;
  h.fill_input(0, [](int64_t i) { return double(i + 1); });

  h.node.inputs.mode = LoopMode::Record;
  h.run(64);
  REQUIRE(h.loop().size() == 64);

  // Quarter-note quantification, with a quarter line landing halfway through
  // the next tick.
  h.node.inputs.quantif.value = 0.25f;
  h.node.inputs.mode = LoopMode::Play;

  auto tk = h.tick(64);
  tk.musical_start_position = 0.5;
  tk.musical_end_position = 1.5;
  tk.musical_start_last_bar = 0.;
  tk.musical_end_last_bar = 0.;
  h.reset_output();
  h.run(tk);

  // First half: still recording, so the input is echoed. Second half: playback.
  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] != Approx(looper_harness::sentinel));
  for(int i = 32; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(h.loop()[i - 32]));
}

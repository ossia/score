#include <Fx/Looper.hpp>

#include <ossia/dataflow/execution_state.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cmath>
#include <string>

#include <array>
#include <vector>

using Catch::Approx;

namespace
{
using Looper = Nodes::AudioLooper::Node;
using LoopMode = Looper::LoopMode;
using Postaction = Looper::Postaction;
using Passthrough = Looper::Passthrough;

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

TEST_CASE("Looper: stopped with full passthrough, the input passes through untouched",
          "[fx][audio][looper]")
{
  looper_harness h;
  h.node.inputs.passthrough = Passthrough::Full;
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
  h.node.inputs.passthrough = Passthrough::Full;
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
  h.node.inputs.passthrough = Passthrough::None;
  h.fill_input(0, [](int64_t i) { return double(i + 1); });

  h.node.inputs.mode = LoopMode::Record;
  h.run(64);

  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(0.));
}

// None means none: not in Stop either, which used to bypass whatever the
// control said.
TEST_CASE("Looper: no passthrough is silent while stopped", "[fx][audio][looper]")
{
  looper_harness h;
  h.node.inputs.passthrough = Passthrough::None;
  h.fill_input(0, [](int64_t i) { return double(i + 1); });

  h.node.inputs.mode = LoopMode::Stop;
  h.run(64);

  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(0.));
}

TEST_CASE("Looper: no passthrough is silent in play with nothing recorded",
          "[fx][audio][looper]")
{
  looper_harness h;
  h.node.inputs.passthrough = Passthrough::None;
  h.fill_input(0, [](int64_t i) { return double(i + 1); });

  h.node.inputs.mode = LoopMode::Play;
  h.run(64);

  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(0.));
}

// Record passthrough is the historical behaviour of the toggle when on: heard
// while recording, and playback is the loop on its own.
TEST_CASE("Looper: record passthrough plays the loop without the input",
          "[fx][audio][looper]")
{
  looper_harness h;
  h.node.inputs.passthrough = Passthrough::Record;
  h.fill_input(0, [](int64_t i) { return double(i + 1); });

  h.node.inputs.mode = LoopMode::Record;
  h.run(64);
  REQUIRE(h.loop().size() == 64);

  h.node.inputs.mode = LoopMode::Play;
  h.fill_input(0, [](int64_t) { return 100.; });
  h.reset_output();
  h.run(64);

  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(h.loop()[i]));
}

TEST_CASE("Looper: full passthrough mixes the input over the loop",
          "[fx][audio][looper]")
{
  looper_harness h;
  h.node.inputs.passthrough = Passthrough::Record;
  h.fill_input(0, [](int64_t i) { return double(i + 1); });

  h.node.inputs.mode = LoopMode::Record;
  h.run(64);
  REQUIRE(h.loop().size() == 64);

  h.node.inputs.passthrough = Passthrough::Full;
  h.node.inputs.mode = LoopMode::Play;
  h.fill_input(0, [](int64_t) { return 100.; });
  h.reset_output();
  h.run(64);

  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(h.loop()[i] + 100.));
}

TEST_CASE("Looper: the first buffer, with nothing recorded, is a passthrough",
          "[fx][audio][looper]")
{
  looper_harness h;
  h.node.inputs.passthrough = Passthrough::Full;
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

namespace
{
// The harness above leaves the musical grid at zero, which is enough for the
// sample machinery but says nothing about quantification or the post-action.
// This one walks bars: 4/4 at 120bpm, so a bar is 2 seconds.
struct musical_harness : looper_harness
{
  double tempo{120.};
  int upper{4}, lower{4};

  explicit musical_harness(int bs = 9600)
      : looper_harness{1, bs, 48000}
  {
  }

  double quarters_per_sample() const { return (tempo / 60.) / 48000.; }
  int64_t bar_samples() const
  {
    return int64_t(48000. * 4. * (double(upper) / lower) * (60. / tempo));
  }

  ossia::token_request mtick(int64_t frames)
  {
    ossia::token_request tk;
    tk.prev_date = ossia::time_value{now};
    tk.date = ossia::time_value{now + frames};
    tk.start_sample = 0;
    tk.length_sample = frames;
    tk.tempo = tempo;
    tk.signature = {uint16_t(upper), uint16_t(lower)};

    const double qps = quarters_per_sample();
    const double bar_q = 4. * (double(upper) / lower);
    tk.musical_start_position = now * qps;
    tk.musical_end_position = (now + frames) * qps;
    tk.musical_start_last_bar = std::floor(tk.musical_start_position / bar_q) * bar_q;
    tk.musical_end_last_bar = std::floor(tk.musical_end_position / bar_q) * bar_q;
    tk.musical_start_last_signature = 0.;
    now += frames;
    return tk;
  }

  //! Runs `bars` bars, returning the summed magnitude written per bar.
  std::vector<double> run_bars(double bars)
  {
    std::vector<double> per_bar;
    const int64_t total = int64_t(bars * bar_samples());
    int64_t done = 0;
    double acc = 0.;
    int64_t in_bar = 0;
    while(done < total)
    {
      const int64_t n = std::min<int64_t>(buffer_size, total - done);
      reset_output();
      run(mtick(n));
      for(int64_t i = 0; i < n; i++)
        acc += std::abs(out_buf[0][i]);
      done += n;
      in_bar += n;
      if(in_bar >= bar_samples())
      {
        per_bar.push_back(acc);
        acc = 0.;
        in_bar = 0;
      }
    }
    if(in_bar > 0)
      per_bar.push_back(acc);
    return per_bar;
  }
};
}

TEST_CASE("Looper: a recording ended by hand keeps the length it was played for",
          "[fx][audio][looper][musical]")
{
  // The bar count says when the post-action takes over. It is not the length of
  // every loop: a recording stopped by hand is as long as it was played for.
  // Stretching it to the bar count is heard as silence at the end of the loop.
  musical_harness h;
  h.node.inputs.quantif.value = 0.25f; // 4th
  h.node.inputs.postaction_bars.value = 4;
  h.node.inputs.postaction = Postaction::Play;
  h.node.inputs.passthrough = Passthrough::None;

  h.node.inputs.mode = LoopMode::Stop;
  h.run_bars(1);

  h.fill_input(0, [](int64_t) { return 1.0; });
  h.node.inputs.mode = LoopMode::Record;
  h.run_bars(3);

  h.reset_input();
  h.node.inputs.mode = LoopMode::Play;
  const auto out = h.run_bars(4);

  CHECK(h.loop().size() == Approx(3. * h.bar_samples()).margin(h.buffer_size));

  // Every bar of playback carries the loop: none of them is the padding.
  REQUIRE(out.size() >= 4);
  for(std::size_t i = 0; i < 4; i++)
    CHECK(out[i] > 0.9 * h.bar_samples());
}

TEST_CASE("Looper: a quantification point on the tick's first sample switches the mode",
          "[fx][audio][looper][musical]")
{
  // A bar line falls exactly on a buffer boundary whenever the bar divides
  // evenly into the buffer, which at 120bpm and 48kHz is every buffer of 256
  // frames or fewer. Refusing a point there leaves the mode waiting for one
  // that never comes, and the looper never records or plays anything at all.
  musical_harness h;
  h.node.inputs.quantif.value = 1.0f; // Whole: one bar
  h.node.inputs.postaction_bars.value = 0;
  h.node.inputs.passthrough = Passthrough::None;
  REQUIRE(h.bar_samples() % h.buffer_size == 0);

  h.node.inputs.mode = LoopMode::Stop;
  h.run_bars(1);

  h.fill_input(0, [](int64_t) { return 1.0; });
  h.node.inputs.mode = LoopMode::Record;
  h.run_bars(2);

  CHECK(h.node.state.actualMode == LoopMode::Record);
  CHECK(h.loop().size() == Approx(2. * h.bar_samples()).margin(h.buffer_size));
}

TEST_CASE("Looper: the post-action takes over after the bars it was given",
          "[fx][audio][looper][musical]")
{
  // What the bar count is for: recording hands over to play or overdub on its
  // own, without the mode being touched.
  musical_harness h;
  h.node.inputs.quantif.value = 1.0f;
  h.node.inputs.postaction_bars.value = 2;
  h.node.inputs.postaction = Postaction::Play;
  h.node.inputs.passthrough = Passthrough::None;

  h.node.inputs.mode = LoopMode::Stop;
  h.run_bars(1);

  h.fill_input(0, [](int64_t) { return 1.0; });
  h.node.inputs.mode = LoopMode::Record;
  h.run_bars(1.5);
  CHECK(h.node.state.actualMode == LoopMode::Record);
  h.run_bars(0.5);

  // The mode control is never touched again from here.
  h.reset_input();
  h.run_bars(1); // the bar the hand-over falls in is part recording, so silent
  CHECK(h.node.state.actualMode == LoopMode::Play);
  CHECK(h.loop().size() == Approx(2. * h.bar_samples()).margin(h.buffer_size));

  const auto out = h.run_bars(2);
  REQUIRE(out.size() >= 2);
  for(std::size_t i = 0; i < 2; i++)
    CHECK(out[i] > 0.9 * h.bar_samples());
}

TEST_CASE("Looper: a post-action recording is trimmed to the bars it was given",
          "[fx][audio][looper][musical]")
{
  // The other side of the first case: when the post-action does end it, the
  // bar count is the length, give or take the rounding of one bar.
  musical_harness h;
  h.node.inputs.quantif.value = 1.0f;
  h.node.inputs.postaction_bars.value = 2;
  h.node.inputs.postaction = Postaction::Play;
  h.node.inputs.passthrough = Passthrough::None;

  h.node.inputs.mode = LoopMode::Stop;
  h.run_bars(1);
  h.fill_input(0, [](int64_t) { return 1.0; });
  h.node.inputs.mode = LoopMode::Record;
  h.run_bars(4); // twice what it was given: the post-action stops it at two

  CHECK(h.loop().size() == Approx(2. * h.bar_samples()).margin(h.buffer_size));
}

TEST_CASE("Looper: record passthrough is heard only while taking something in",
          "[fx][audio][looper]")
{
  // "Record passthrough" is the input while recording or overdubbing. Stopped,
  // or playing, the loop is what comes out -- alone.
  looper_harness h;
  h.node.inputs.passthrough = Passthrough::Record;
  h.fill_input(0, [](int64_t) { return 1.0; });

  SECTION("stopped, nothing comes out")
  {
    h.node.inputs.mode = LoopMode::Stop;
    h.run(64);
    for(int i = 0; i < 64; i++)
      CHECK(h.out_buf[0][i] == Approx(0.));
  }

  SECTION("recording, the input comes out")
  {
    h.node.inputs.mode = LoopMode::Record;
    h.run(64);
    for(int i = 0; i < 64; i++)
      CHECK(h.out_buf[0][i] == Approx(1.));
  }

  SECTION("playing, the loop alone comes out")
  {
    h.node.inputs.mode = LoopMode::Record;
    h.run(64);
    h.node.inputs.mode = LoopMode::Play;
    h.reset_output();
    h.run(64);
    // what comes out is the loop as it was kept, with nothing added on top
    for(int i = 0; i < 64; i++)
      CHECK(h.out_buf[0][i] == Approx(h.loop()[i]));
  }

  SECTION("playing with nothing recorded, nothing comes out")
  {
    h.node.inputs.mode = LoopMode::Play;
    h.run(64);
    for(int i = 0; i < 64; i++)
      CHECK(h.out_buf[0][i] == Approx(0.));
  }
}

TEST_CASE("Looper: overdub can hand over to play without rewinding",
          "[fx][audio][looper]")
{
  // Reading the loop from the top on every change of mode is heard as a jump
  // when overdubbing gives way to playing. Which of the two happens is the
  // Restart setting's business; a recording always starts its buffer again,
  // there being nothing yet to carry on from.
  using Restart = Looper::Restart;

  const auto record_two_buffers = [](looper_harness& h) {
    h.node.inputs.passthrough = Passthrough::None;
    h.fill_input(0, [](int64_t i) { return double(i + 1); });
    h.node.inputs.mode = LoopMode::Record;
    h.run(64);
    h.run(64);
    REQUIRE(h.loop().size() == 128);
  };

  SECTION("carrying on, the loop is read from where overdubbing left it")
  {
    looper_harness h;
    h.node.inputs.restart = Restart::Recording;
    record_two_buffers(h);

    h.node.inputs.mode = LoopMode::Overdub;
    h.reset_input();
    h.run(64); // overdubs the first half, position now 64

    h.node.inputs.mode = LoopMode::Play;
    h.reset_output();
    h.run(64);

    // the second half of the loop, not the first
    for(int i = 0; i < 64; i++)
      CHECK(h.out_buf[0][i] == Approx(h.loop()[64 + i]));
  }

  SECTION("restarting, the loop is read from its beginning")
  {
    looper_harness h;
    h.node.inputs.restart = Restart::Always;
    record_two_buffers(h);

    h.node.inputs.mode = LoopMode::Overdub;
    h.reset_input();
    h.run(64);

    h.node.inputs.mode = LoopMode::Play;
    h.reset_output();
    h.run(64);

    for(int i = 0; i < 64; i++)
      CHECK(h.out_buf[0][i] == Approx(h.loop()[i]));
  }

  SECTION("a recording always starts its buffer again, whatever the setting")
  {
    looper_harness h;
    h.node.inputs.restart = Restart::Recording;
    record_two_buffers(h);

    h.node.inputs.mode = LoopMode::Play;
    h.run(64);

    h.fill_input(0, [](int64_t) { return 9.; });
    h.node.inputs.mode = LoopMode::Record;
    h.run(64);
    CHECK(h.loop().size() == 64);
  }
}

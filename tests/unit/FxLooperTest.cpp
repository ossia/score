#include <Fx/Looper/AudioLooper.hpp>

#include <ossia/dataflow/execution_state.hpp>

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include <dr_wav.h>

#include <catch2/catch_approx.hpp>
#include "AllocCounter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <memory>
#include <new>
#include <string>

#include <array>
#include <vector>

using Catch::Approx;

namespace
{
using Looper = Nodes::AudioLooper::Node;
using LoopMode = Looper::LoopMode;
using AfterRecord = Nodes::Looper::AfterRecord;
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
  double tempo{120.};

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
    node.inputs.quantif.value = halp::grid_value{};
    // Most cases here are about what happens on the grid, not just after it.
    node.inputs.late.value = 0;
    // The cases here were written when a change of mode read the loop from
    // its start and the two oldest passes merged past Max layers.
    node.inputs.start_from = Nodes::Looper::StartFrom::LoopStart;
    node.inputs.at_max_layers = Nodes::Looper::AtMaxLayers::MergeOldest;

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
    tk.tempo = tempo;
    now += frames;
    // The engine's clock is at the end of the buffer while it runs.
    st.samples_since_start = now;
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

  //! The first pass of the loop, as the test sees it: as long as the loop.
  struct loop_view
  {
    const Looper& node;
    int chan;
    int64_t size() const noexcept { return node.layers.length; }
    float operator[](int64_t i) const noexcept { return node.layer_data(0, chan)[i]; }
  };
  loop_view loop(int chan = 0) const { return {node, chan}; }
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

TEST_CASE("Looper: overdubbing an empty loop records it", "[fx][audio][looper]")
{
  looper_harness h;
  // A buffer barely larger than the tick: the recording fills it and stops.
  h.node.layers.allocate(2, 2, 100);

  h.fill_input(0, [](int64_t i) { return double(i + 1); });
  h.node.inputs.mode = LoopMode::Overdub;
  h.run(64);
  CHECK(h.loop().size() == 64);
  CHECK(h.node.transport.actualMode == LoopMode::Record);
  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(double(i + 1)));

  // Changing the mode closes the recording; Overdub again layers over it.
  h.node.inputs.mode = LoopMode::Play;
  h.run(64);
  h.node.inputs.mode = LoopMode::Overdub;
  h.run(64);
  CHECK(h.node.transport.actualMode == LoopMode::Overdub);
  CHECK(h.node.layers.count == 2);
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

  // Overdubs are layers of their own: what the loop plays is their mix.
  CHECK(h.node.layers.mix(0, 28) == Approx(1.));
  CHECK(h.node.layers.mix(0, 0) == Approx(0.));
}

TEST_CASE("Looper: playback never writes past the advertised output channels",
          "[fx][audio][looper]")
{
  looper_harness h{2};
  h.fill_input(0, [](int64_t i) { return double(i + 1); });
  h.fill_input(1, [](int64_t i) { return double(i + 1); });

  h.node.inputs.mode = LoopMode::Record;
  h.run(64);
  REQUIRE(h.node.layers.channels == 2);

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

  REQUIRE(h.node.layers.channels == 2);
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
  h.node.inputs.quantif.value = halp::grid_value::note(0.25);
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
  h.node.inputs.quantif.value = halp::grid_value::note(0.25); // the shipped default
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
  h.node.inputs.after_record = AfterRecord::Play;
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
  CHECK(h.node.transport.playbackPos == 32);
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
  h.node.inputs.quantif.value = halp::grid_value::note(0.25);
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
  bool accumulate{false};
  double position{};

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
    if(accumulate)
    {
      // Positions go on from where they were: a tempo change bends the time
      // from then on, not what came before.
      tk.musical_start_position = position;
      tk.musical_end_position = position + frames * qps;
      position += frames * qps;
    }
    else
    {
      tk.musical_start_position = now * qps;
      tk.musical_end_position = (now + frames) * qps;
    }
    tk.musical_start_last_bar = std::floor(tk.musical_start_position / bar_q) * bar_q;
    tk.musical_end_last_bar = std::floor(tk.musical_end_position / bar_q) * bar_q;
    tk.musical_start_last_signature = 0.;
    now += frames;
    st.samples_since_start = now;
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
  h.node.inputs.quantif.value = halp::grid_value::note(0.25); // 4th
  h.node.inputs.postaction_bars.value = 4;
  h.node.inputs.after_record = AfterRecord::Play;
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
  h.node.inputs.quantif.value = halp::grid_value::bars(1); // Whole: one bar
  h.node.inputs.postaction_bars.value = 0;
  h.node.inputs.passthrough = Passthrough::None;
  REQUIRE(h.bar_samples() % h.buffer_size == 0);

  h.node.inputs.mode = LoopMode::Stop;
  h.run_bars(1);

  h.fill_input(0, [](int64_t) { return 1.0; });
  h.node.inputs.mode = LoopMode::Record;
  h.run_bars(2);

  CHECK(h.node.transport.actualMode == LoopMode::Record);
  CHECK(h.loop().size() == Approx(2. * h.bar_samples()).margin(h.buffer_size));
}

TEST_CASE("Looper: the post-action takes over after the bars it was given",
          "[fx][audio][looper][musical]")
{
  // What the bar count is for: recording hands over to play or overdub on its
  // own, without the mode being touched.
  musical_harness h;
  h.node.inputs.quantif.value = halp::grid_value::bars(1);
  h.node.inputs.postaction_bars.value = 2;
  h.node.inputs.after_record = AfterRecord::Play;
  h.node.inputs.passthrough = Passthrough::None;

  h.node.inputs.mode = LoopMode::Stop;
  h.run_bars(1);

  h.fill_input(0, [](int64_t) { return 1.0; });
  h.node.inputs.mode = LoopMode::Record;
  h.run_bars(1.5);
  CHECK(h.node.transport.actualMode == LoopMode::Record);
  h.run_bars(0.5);

  // The mode control is never touched again from here.
  h.reset_input();
  h.run_bars(1); // the bar the hand-over falls in is part recording, so silent
  CHECK(h.node.transport.actualMode == LoopMode::Play);
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
  h.node.inputs.quantif.value = halp::grid_value::bars(1);
  h.node.inputs.postaction_bars.value = 2;
  h.node.inputs.after_record = AfterRecord::Play;
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
  using StartFrom = Nodes::Looper::StartFrom;

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
    h.node.inputs.start_from = StartFrom::WhereItWas;
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
    h.node.inputs.start_from = StartFrom::LoopStart;
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
    h.node.inputs.start_from = StartFrom::WhereItWas;
    record_two_buffers(h);

    h.node.inputs.mode = LoopMode::Play;
    h.run(64);

    h.fill_input(0, [](int64_t) { return 9.; });
    h.node.inputs.mode = LoopMode::Record;
    h.run(64);
    CHECK(h.loop().size() == 64);
  }
}

// ---------------------------------------------------------------------------
// Layers, undo, feedback, memory and takes.
// ---------------------------------------------------------------------------

namespace
{
//! Records `len` frames of a constant, then overdubs one pass of another
//! constant over the whole loop, ending in Play.
void record_and_overdub(looper_harness& h, int64_t len, double base, double over)
{
  h.fill_input(0, [](int64_t) { return 0.; });
  for(auto& v : h.in_buf[0])
    v = base;
  h.node.inputs.mode = LoopMode::Record;
  for(int64_t done = 0; done < len; done += h.buffer_size)
    h.run(std::min<int64_t>(h.buffer_size, len - done));

  for(auto& v : h.in_buf[0])
    v = over;
  h.node.inputs.mode = LoopMode::Overdub;
  for(int64_t done = 0; done < len; done += h.buffer_size)
    h.run(std::min<int64_t>(h.buffer_size, len - done));

  h.reset_input();
  h.node.inputs.mode = LoopMode::Play;
  h.run(h.buffer_size);
}

void press(auto& button, looper_harness& h)
{
  button.value.emplace();
  h.run(h.buffer_size);
  button.value.reset();
}
}

TEST_CASE("Looper: an overdub is a layer of its own, which undo hides and redo brings back",
          "[fx][audio][looper][layers]")
{
  looper_harness h{1, 64};
  record_and_overdub(h, 512, 0.25, 0.5);
  REQUIRE(h.node.layers.count == 2);

  // Away from the faded edges, the loop plays both passes.
  CHECK(h.node.layers.mix(0, 256) == Approx(0.75));
  // The first pass is untouched by the overdub.
  CHECK(h.loop()[256] == Approx(0.25));

  press(h.node.inputs.undo, h);
  CHECK(h.node.layers.count == 1);
  CHECK(h.node.layers.mix(0, 256) == Approx(0.25));

  press(h.node.inputs.redo, h);
  CHECK(h.node.layers.count == 2);
  CHECK(h.node.layers.mix(0, 256) == Approx(0.75));
}

TEST_CASE("Looper: undoing the first recording empties the loop, and redo restores it",
          "[fx][audio][looper][layers]")
{
  looper_harness h{1, 64};
  h.node.inputs.passthrough = Passthrough::None;
  record_and_overdub(h, 512, 0.25, 0.5);

  press(h.node.inputs.undo, h);
  press(h.node.inputs.undo, h);
  CHECK(h.node.layers.empty());
  h.reset_output();
  h.run(64);
  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(0.));

  press(h.node.inputs.redo, h);
  CHECK(!h.node.layers.empty());
  CHECK(h.node.layers.mix(0, 256) == Approx(0.25));
}

TEST_CASE("Looper: a new overdub after an undo drops what redo would have brought back",
          "[fx][audio][looper][layers]")
{
  looper_harness h{1, 64};
  record_and_overdub(h, 512, 0.25, 0.5);
  press(h.node.inputs.undo, h);
  REQUIRE(h.node.layers.total == 2);

  for(auto& v : h.in_buf[0])
    v = 0.125;
  h.node.inputs.mode = LoopMode::Overdub;
  h.run(64);
  CHECK(h.node.layers.count == 2);
  CHECK(h.node.layers.total == 2);
  press(h.node.inputs.redo, h);
  CHECK(h.node.layers.count == 2);
}

TEST_CASE("Looper: clear forgets every pass", "[fx][audio][looper][layers]")
{
  looper_harness h{1, 64};
  record_and_overdub(h, 512, 0.25, 0.5);
  press(h.node.inputs.clear, h);
  CHECK(h.node.layers.empty());
  CHECK(h.node.layers.length == 0);
  press(h.node.inputs.redo, h);
  CHECK(h.node.layers.empty());
}

TEST_CASE("Looper: undo during an overdub drops the pass in progress",
          "[fx][audio][looper][layers]")
{
  looper_harness h{1, 64};
  record_and_overdub(h, 512, 0.25, 0.5);
  for(auto& v : h.in_buf[0])
    v = 1.;
  h.node.inputs.mode = LoopMode::Overdub;
  h.run(64);
  REQUIRE(h.node.layers.count == 3);

  h.node.inputs.undo.value.emplace();
  h.run(64);
  h.node.inputs.undo.value.reset();
  // The pass was dropped, and overdubbing carries on in a new one.
  CHECK(h.node.layers.count == 3);
  CHECK(h.node.layers.top()->written == 64);
}

TEST_CASE("Looper: feedback lets older passes decay as an overdub goes round",
          "[fx][audio][looper][layers]")
{
  looper_harness h{1, 64};
  h.node.inputs.feedback.value = 0.5f;
  h.fill_input(0, [](int64_t) { return 1.; });
  h.node.inputs.mode = LoopMode::Record;
  h.run(64);
  h.run(64);

  h.reset_input();
  h.node.inputs.mode = LoopMode::Overdub;
  for(int i = 0; i < 4; i++) // two whole rounds
    h.run(64);

  CHECK(h.node.layers.layers[0].gain == Approx(0.25));

  // Undoing the overdub gives the first pass its gain back.
  h.node.inputs.mode = LoopMode::Play;
  h.run(64);
  press(h.node.inputs.undo, h);
  CHECK(h.node.layers.layers[0].gain == Approx(1.));
}

TEST_CASE("Looper: past the last layer, the two oldest passes merge and the sum is kept",
          "[fx][audio][looper][layers]")
{
  looper_harness h{1, 64};
  h.node.layers.allocate(2, 2, 48000);
  record_and_overdub(h, 512, 0.25, 0.5);
  REQUIRE(h.node.layers.count == 2);

  for(auto& v : h.in_buf[0])
    v = 0.125;
  h.node.inputs.mode = LoopMode::Overdub;
  for(int i = 0; i < 8; i++)
    h.run(64);
  h.reset_input();
  h.node.inputs.mode = LoopMode::Play;
  h.run(64);

  CHECK(h.node.layers.count == 2);
  CHECK(h.node.layers.mix(0, 256) == Approx(0.875));
}

TEST_CASE("Looper: a full buffer ends the recording and plays what fitted",
          "[fx][audio][looper]")
{
  looper_harness h{1, 64};
  h.node.layers.allocate(1, 2, 150);
  h.node.inputs.passthrough = Passthrough::None;
  h.fill_input(0, [](int64_t i) { return double(i + 1); });

  h.node.inputs.mode = LoopMode::Record;
  h.run(64);
  h.run(64);
  h.reset_output();
  h.run(64); // 22 frames fit, the rest plays the loop

  CHECK(h.node.layers.length == 150);
  CHECK(h.node.transport.actualMode == LoopMode::Play);
  for(int i = 22; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(h.node.layers.mix(0, i - 22)));
}

TEST_CASE("Looper: latency moves what was recorded earlier in the loop",
          "[fx][audio][looper]")
{
  looper_harness h{1, 100, 1000}; // 1 ms is a frame
  h.node.inputs.latency.value = 10;
  h.node.inputs.passthrough = Passthrough::None;
  // Away from the faded edges of a 500 frame loop.
  h.node.inputs.mode = LoopMode::Record;
  for(int i = 0; i < 5; i++)
  {
    h.reset_input();
    if(i == 2)
      h.in_buf[0][50] = 1.;
    h.run(100);
  }
  h.node.inputs.mode = LoopMode::Play;
  h.run(100);

  CHECK(h.loop()[240] == Approx(1.));
  CHECK(h.loop()[250] == Approx(0.));
}

TEST_CASE("Looper: nothing is allocated while recording, overdubbing, undoing and playing",
          "[fx][audio][looper]")
{
  looper_harness h{2, 64};
  h.fill_input(0, [](int64_t i) { return std::sin(i * 0.1); });
  h.fill_input(1, [](int64_t i) { return std::cos(i * 0.1); });

  int allocated = 0;
  {
    alloc_counter::scope counter;
    h.node.inputs.mode = LoopMode::Record;
    for(int i = 0; i < 20; i++)
      h.run(64);
    h.node.inputs.mode = LoopMode::Overdub;
    for(int i = 0; i < 40; i++)
      h.run(64);
    h.node.inputs.mode = LoopMode::Play;
    for(int i = 0; i < 10; i++)
      h.run(64);
    press(h.node.inputs.undo, h);
    press(h.node.inputs.redo, h);
    press(h.node.inputs.clear, h);
    h.node.inputs.mode = LoopMode::Record;
    h.run(64);
    allocated = counter.allocations();
  }
  CHECK(allocated == 0);
}

namespace
{
struct wav_contents
{
  std::vector<float> samples;
  int channels{};
  int rate{};
  float tempo{};
  int beats{};
  bool has_loop{};
};

wav_contents read_wav(const std::string& path)
{
  wav_contents res;
  drwav wav;
  if(!drwav_init_file_with_metadata(&wav, path.c_str(), 0, nullptr))
    return res;
  res.channels = wav.channels;
  res.rate = wav.sampleRate;
  for(drwav_uint32 i = 0; i < wav.metadataCount; i++)
  {
    const auto& m = wav.pMetadata[i];
    if(m.type == drwav_metadata_type_acid)
    {
      res.tempo = m.data.acid.tempo;
      res.beats = int(m.data.acid.numBeats);
    }
    else if(m.type == drwav_metadata_type_smpl)
      res.has_loop = m.data.smpl.sampleLoopCount > 0;
  }
  res.samples.resize(wav.totalPCMFrameCount * wav.channels);
  drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, res.samples.data());
  drwav_uninit(&wav);
  return res;
}
}

TEST_CASE("Looper: a take is written as a looping WAV with its tempo, as it plays",
          "[fx][audio][looper][takes]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  musical_harness h{4800};
  h.tempo = 97.;
  h.node.inputs.quantif.value = halp::grid_value{};
  h.node.inputs.passthrough = Passthrough::None;

  auto writer = std::make_shared<Nodes::Looper::take_writer>();
  writer->folder = dir.path();
  writer->prefix = "test";
  writer->extension = "wav";
  h.node.writer = writer;
  h.node.worker.request = [&](Nodes::Looper::take_job job) {
    Looper::worker_t::work(std::move(job))(h.node);
  };

  // Two bars at 97 bpm.
  const int64_t len = int64_t(std::round(2. * 4. * 60. / 97. * 48000.));
  h.fill_input(0, [](int64_t i) { return 0.5 * std::sin(i * 0.05); });
  h.node.inputs.mode = LoopMode::Record;
  for(int64_t done = 0; done < len; done += 4800)
    h.run(h.mtick(std::min<int64_t>(4800, len - done)));
  h.reset_input();
  h.node.inputs.mode = LoopMode::Play;
  // Flattening is spread over a few ticks.
  for(int i = 0; i < 6; i++)
    h.run(h.mtick(4800));
  REQUIRE(h.node.layers.length == len);

  const auto files = QDir{dir.path()}.entryList({"*.wav"});
  REQUIRE(files.size() == 1);
  CHECK(files[0] == "test-take1.wav");

  const auto wav = read_wav(QString(dir.path() + "/" + files[0]).toStdString());
  CHECK(wav.rate == 48000);
  CHECK(wav.tempo == Approx(97.f));
  CHECK(wav.beats == 8);
  CHECK(wav.has_loop);
  REQUIRE(int64_t(wav.samples.size()) == len * wav.channels);
  // The input as it was played: every tick fed the same buffer, the last one
  // in part. The loop's edges are faded over 128 frames.
  const int64_t fade = Nodes::Looper::audio_layers::fade_frames;
  for(int64_t i = 0; i < len; i += 97)
  {
    double expected = 0.5 * std::sin((i % 4800) * 0.05);
    if(i < fade)
      expected *= double(i) / fade;
    else if(i >= len - fade)
      expected *= double(len - i) / fade;
    INFO(i);
    CHECK(wav.samples[i * wav.channels] == Approx(expected).margin(1e-5));
  }
}

TEST_CASE("Looper: a stop just after a grid point ends the recording on that point",
          "[fx][audio][looper][musical]")
{
  for(const int late_ms : {0, 250})
  {
    musical_harness h;
    h.node.inputs.quantif.value = halp::grid_value::bars(1);
    h.node.inputs.postaction_bars.value = 0;
    h.node.inputs.passthrough = Passthrough::None;
    h.node.inputs.late.value = late_ms;

    h.node.inputs.mode = LoopMode::Stop;
    h.run_bars(1);
    h.fill_input(0, [](int64_t) { return 1.0; });
    h.node.inputs.mode = LoopMode::Record;
    h.run_bars(2);
    // One tick, 200 ms, past the bar line: the footswitch was pressed late.
    h.run(h.mtick(h.buffer_size));

    h.node.inputs.mode = LoopMode::Play;
    h.run(h.mtick(h.buffer_size));

    if(late_ms == 0)
    {
      // Waits for the next bar line.
      CHECK(h.node.transport.actualMode == LoopMode::Record);
    }
    else
    {
      CHECK(h.node.transport.actualMode == LoopMode::Play);
      CHECK(h.loop().size() == 2 * h.bar_samples());
      // Playback carries on where the loop would be had the stop been on time.
      CHECK(h.node.transport.playbackPos == 2 * h.buffer_size);
    }
  }
}

TEST_CASE("Looper: an overdub asked for just after a grid point ends a recording keeps "
          "what was played since the point",
          "[fx][audio][looper][musical]")
{
  musical_harness h{480};
  h.node.inputs.quantif.value = halp::grid_value::bars(1);
  h.node.inputs.postaction_bars.value = 0;
  h.node.inputs.passthrough = Passthrough::None;
  h.node.inputs.late.value = 120;

  h.node.inputs.mode = LoopMode::Stop;
  h.run_bars(1);
  h.node.inputs.mode = LoopMode::Record;
  h.run_bars(2);
  // A hit on the bar line, and the footswitch pressed 100 ms later.
  h.fill_input(0, [](int64_t) { return 1.0; });
  h.run_bars(0.05);
  h.node.inputs.mode = LoopMode::Overdub;
  h.run(h.mtick(h.buffer_size));
  REQUIRE(h.node.transport.actualMode == LoopMode::Overdub);
  REQUIRE(h.loop().size() == 2 * h.bar_samples());
  h.reset_input();
  h.run_bars(2);
  h.node.inputs.mode = LoopMode::Play;
  h.run_bars(0.5);
  REQUIRE(h.node.layers.count == 2);

  // The hit is the pass's from the bar line on, at full level: the pass
  // carries on what the recording was taking in.
  const float* pass = h.node.layer_data(1, 0);
  const int64_t played = int64_t(h.bar_samples() * 0.05) + h.buffer_size;
  for(int64_t i : {int64_t(0), int64_t(64), int64_t(128), int64_t(1000), int64_t(4799), played - 1})
  {
    INFO(i);
    CHECK(pass[i] == Approx(1.0));
  }
  CHECK(pass[played + 10] == Approx(0.0));
}

TEST_CASE("Looper: loopers of one sync group share the loop and its phase",
          "[fx][audio][looper][sync]")
{
  looper_harness a{1, 100}, b{1, 100};
  a.node.inputs.sync.value = Nodes::Looper::SyncGroup::B;
  b.node.inputs.sync.value = Nodes::Looper::SyncGroup::B;
  a.fill_input(0, [](int64_t) { return 1.; });
  b.fill_input(0, [](int64_t) { return 1.; });

  auto both = [&](int ticks) {
    for(int i = 0; i < ticks; i++)
    {
      a.run(100);
      b.run(100);
    }
  };

  // a records 1000 frames and leads.
  a.node.inputs.mode = LoopMode::Record;
  both(10);
  a.node.inputs.mode = LoopMode::Play;
  both(3);
  REQUIRE(a.node.layers.length == 1000);

  // b records 1900 frames: its loop becomes two of a's.
  b.node.inputs.mode = LoopMode::Record;
  both(19);
  b.node.inputs.mode = LoopMode::Play;
  both(1);
  CHECK(b.node.layers.length == 2000);

  // Both go round in phase: b's position is a's, or a's plus one loop of a.
  for(int i = 0; i < 25; i++)
  {
    both(1);
    CHECK(b.node.transport.playbackPos % 1000 == a.node.transport.playbackPos);
  }

  // A third, short recording becomes half of the group's loop.
  looper_harness c{1, 100};
  c.node.inputs.sync.value = Nodes::Looper::SyncGroup::B;
  c.fill_input(0, [](int64_t) { return 1.; });
  c.now = a.now;
  c.node.inputs.mode = LoopMode::Record;
  for(int i = 0; i < 6; i++)
  {
    a.run(100);
    c.run(100);
  }
  c.node.inputs.mode = LoopMode::Play;
  a.run(100);
  c.run(100);
  CHECK(c.node.layers.length == 500);
  CHECK(c.node.transport.playbackPos == a.node.transport.playbackPos % 500);

  // Clearing the leader frees the group.
  a.node.inputs.clear.value.emplace();
  a.run(100);
  CHECK(a.node.sync.current()->leader.load() == nullptr);
}

TEST_CASE("Looper: capture makes a loop of the last bars heard, and plays it",
          "[fx][audio][looper][capture]")
{
  // 1000 frames a second: a bar at 120 bpm is 2000 frames.
  looper_harness h{1, 100, 1000};
  h.node.allocate_ring(1, 3000);
  h.node.inputs.capture_bars.value = 1;
  h.node.inputs.passthrough = Passthrough::None;
  h.node.inputs.mode = LoopMode::Stop;

  int64_t t = 0;
  for(int i = 0; i < 30; i++)
  {
    for(int j = 0; j < 100; j++)
      h.in_buf[0][j] = double(t + j) / 10000.;
    t += 100;
    h.run(100);
  }

  h.reset_input();
  h.node.inputs.capture.value.emplace();
  h.run(100);
  h.node.inputs.capture.value.reset();

  REQUIRE(h.node.layers.length == 2000);
  CHECK(h.node.transport.actualMode == LoopMode::Play);
  // The loop is frames 1000 to 3000 of the input, faded at its edges.
  for(int64_t i = 200; i < 1800; i += 50)
    CHECK(h.loop()[i] == Approx((1000. + i) / 10000.).margin(1e-6));
  // The tick of the capture played the loop's start.
  CHECK(h.node.transport.playbackPos == 100);
}

TEST_CASE("Looper: the latency the engine reports is made up for when asked",
          "[fx][audio][looper]")
{
  for(const bool engine : {false, true})
  {
    looper_harness h{1, 100, 1000};
    h.st.inputLatency = 4;
    h.st.outputLatency = 6;
    h.node.inputs.latency.value = 0;
    h.node.inputs.engine_latency.value = engine;
    h.node.inputs.passthrough = Passthrough::None;
    h.node.inputs.mode = LoopMode::Record;
    for(int i = 0; i < 5; i++)
    {
      h.reset_input();
      if(i == 2)
        h.in_buf[0][50] = 1.;
      h.run(100);
    }
    h.node.inputs.mode = LoopMode::Play;
    h.run(100);

    CHECK(h.loop()[engine ? 240 : 250] == Approx(1.));
  }
}

TEST_CASE("Looper: a sound file dropped on the looper becomes the loop",
          "[fx][audio][looper]")
{
  looper_harness h{2, 100, 1000};
  h.node.inputs.passthrough = Passthrough::None;
  h.node.inputs.mode = LoopMode::Play;

  // A mono file at half the looper's rate: it is resampled, and both channels
  // get it.
  std::vector<float> mono(400);
  for(int i = 0; i < 400; i++)
    mono[i] = float(i) / 400.f;
  const float* chans[1]{mono.data()};
  h.node.inputs.file.soundfile.data = chans;
  h.node.inputs.file.soundfile.frames = 400;
  h.node.inputs.file.soundfile.channels = 1;
  h.node.inputs.file.soundfile.rate = 500;

  h.run(100);
  REQUIRE(h.node.layers.length == 800);
  CHECK(h.node.transport.actualMode == LoopMode::Play);
  CHECK(h.node.layers.mix(0, 400) == Approx(200.f / 400.f));
  CHECK(h.node.layers.mix(1, 401) == Approx(200.5f / 400.f));

  // The same file again changes nothing; another one replaces the loop.
  h.run(100);
  CHECK(h.node.layers.length == 800);
  h.node.inputs.file.soundfile.frames = 200;
  h.run(100);
  CHECK(h.node.layers.length == 400);
}

TEST_CASE("Looper: the loop goes round with no output channel", "[fx][audio][looper]")
{
  looper_harness h{1, 64};
  h.fill_input(0, [](int64_t) { return 1.; });
  h.node.inputs.mode = LoopMode::Record;
  h.run(64);
  h.run(36);
  REQUIRE(h.node.layers.length == 100);

  // Nothing is patched in any more: the bus has no channel.
  h.channels = 0;
  h.node.inputs.mode = LoopMode::Play;
  h.run(64);
  CHECK(h.node.transport.playbackPos == 64);
  h.run(64);
  CHECK(h.node.transport.playbackPos == 28);
}

TEST_CASE("Looper: undoing an overdub brings back what feedback took away",
          "[fx][audio][looper][layers]")
{
  looper_harness h{1, 64};
  h.node.inputs.feedback.value = 0.f;
  record_and_overdub(h, 512, 0.25, 0.5);
  REQUIRE(h.node.layers.layers[0].gain == Approx(0.f));
  press(h.node.inputs.undo, h);
  CHECK(h.node.layers.layers[0].gain == Approx(1.f));
  CHECK(h.node.layers.mix(0, 256) == Approx(0.25));
  press(h.node.inputs.redo, h);
  CHECK(h.node.layers.layers[0].gain == Approx(0.f));
}

TEST_CASE("Looper: a stop just after the bar line keeps a sync group on the grid",
          "[fx][audio][looper][sync]")
{
  musical_harness h;
  h.node.inputs.quantif.value = halp::grid_value::bars(1);
  h.node.inputs.postaction_bars.value = 0;
  h.node.inputs.late.value = 250;
  h.node.inputs.sync.value = Nodes::Looper::SyncGroup::C;
  h.node.inputs.mode = LoopMode::Stop;
  h.run_bars(1);
  h.fill_input(0, [](int64_t) { return 1.0; });
  h.node.inputs.mode = LoopMode::Record;
  h.run_bars(2);
  h.run(h.mtick(h.buffer_size));
  h.node.inputs.mode = LoopMode::Play;
  h.run(h.mtick(h.buffer_size));

  // The loop restarted on the bar line, one tick before the stop was asked.
  auto* g = h.node.sync.current();
  REQUIRE(g);
  CHECK(g->origin.load() == 3 * h.bar_samples());
  CHECK(h.node.transport.playbackPos == 2 * h.buffer_size);
}

TEST_CASE("Looper: overdub on an empty loop hands over to overdubbing after its bars",
          "[fx][audio][looper][musical]")
{
  // A loop of two bars on a grid of one: the hand-over is on the loop's end,
  // and overdubbing goes on from there, in one pass, without waiting for the
  // next bar line -- which is the middle of the loop, and would have read it
  // from its start again there.
  musical_harness h;
  h.node.inputs.quantif.value = halp::grid_value::bars(1);
  h.node.inputs.postaction_bars.value = 2;
  h.node.inputs.after_record = AfterRecord::Play;
  h.node.inputs.mode = LoopMode::Stop;
  h.run_bars(1);
  h.fill_input(0, [](int64_t) { return 0.5; });
  h.node.inputs.mode = LoopMode::Overdub;
  h.run_bars(1);
  CHECK(h.node.transport.actualMode == LoopMode::Record);
  h.run_bars(1);
  h.run_bars(1.5);
  CHECK(h.loop().size() == 2 * h.bar_samples());
  CHECK(h.node.transport.actualMode == LoopMode::Overdub);
  REQUIRE(h.node.layers.count == 2);
  CHECK(h.node.layers.top()->start == 0);
  CHECK(h.node.layers.top()->written == int64_t(1.5 * h.bar_samples()));
  CHECK(h.node.transport.playbackPos == int64_t(1.5 * h.bar_samples()));
}

TEST_CASE("Looper: a take can be written as its mix, its layers, or both",
          "[fx][audio][looper][takes]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  looper_harness h{1, 64};
  auto writer = std::make_shared<Nodes::Looper::take_writer>();
  writer->folder = dir.path();
  writer->prefix = "t";
  writer->extension = "wav";
  h.node.writer = writer;
  h.node.worker.request = [&](Nodes::Looper::take_job job) {
    Looper::worker_t::work(std::move(job))(h.node);
  };
  h.node.inputs.exported.value = Nodes::Looper::TakeFiles::Both;

  record_and_overdub(h, 512, 0.25, 0.5);
  for(int i = 0; i < 10; i++)
    h.run(64);

  auto files = QDir{dir.path()}.entryList({"*.wav"}, QDir::Files, QDir::Name);
  // The take of the recording, then the one of the overdub.
  CHECK(files.contains("t-take1.wav"));
  CHECK(files.contains("t-take1-layer1.wav"));
  CHECK(files.contains("t-take2.wav"));
  CHECK(files.contains("t-take2-layer1.wav"));
  CHECK(files.contains("t-take2-layer2.wav"));

  const auto layer2 = read_wav(QString(dir.path() + "/t-take2-layer2.wav").toStdString());
  REQUIRE(layer2.samples.size() == 512 * std::size_t(layer2.channels));
  CHECK(layer2.samples[256 * layer2.channels] == Approx(0.5));
  const auto mix = read_wav(QString(dir.path() + "/t-take2.wav").toStdString());
  REQUIRE(mix.samples.size() == 512 * std::size_t(mix.channels));
  CHECK(mix.samples[256 * mix.channels] == Approx(0.75));

  // What the view offers to send or drag: every file of the last take.
  Nodes::Looper::view_message v{};
  v.set_take(QString(dir.path() + "/t-take2").toStdString(), "wav", true, 2);
  const auto paths = v.take_files();
  REQUIRE(paths.size() == 3);
  CHECK(paths[2] == QString(dir.path() + "/t-take2-layer2.wav").toStdString());
}

TEST_CASE("Looper: a take keeps the tempo it was recorded at, whatever the tempo is now",
          "[fx][audio][looper][takes][tempo]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  musical_harness h{4800};
  h.accumulate = true;
  h.node.inputs.quantif.value = halp::grid_value{};
  auto writer = std::make_shared<Nodes::Looper::take_writer>();
  writer->folder = dir.path();
  writer->prefix = "t";
  writer->extension = "wav";
  h.node.writer = writer;
  h.node.worker.request = [&](Nodes::Looper::take_job job) {
    Looper::worker_t::work(std::move(job))(h.node);
  };

  // Two bars at 120, then the tempo drops to 90 just as the recording ends.
  h.fill_input(0, [](int64_t) { return 0.25; });
  h.node.inputs.mode = LoopMode::Record;
  for(int i = 0; i < 40; i++)
    h.run(h.mtick(4800));
  h.tempo = 90.;
  h.node.inputs.mode = LoopMode::Play;
  for(int i = 0; i < 6; i++)
    h.run(h.mtick(4800));

  const auto wav = read_wav(QString(dir.path() + "/t-take1.wav").toStdString());
  CHECK(wav.tempo == Approx(120.f));
  CHECK(wav.beats == 8);
}

TEST_CASE("Looper: an extending overdub makes the loop longer, by whole loops",
          "[fx][audio][looper][layers]")
{
  looper_harness h{1, 64};
  h.node.inputs.past_end.value = Nodes::Looper::PastEnd::Extend;
  h.node.inputs.start_from = Nodes::Looper::StartFrom::WhereItWas;
  for(auto& v : h.in_buf[0])
    v = 0.25;
  h.node.inputs.mode = LoopMode::Record;
  for(int i = 0; i < 8; i++)
    h.run(64);
  REQUIRE(h.node.layers.length == 512);

  // Overdub from the loop's start for four of its lengths.
  for(auto& v : h.in_buf[0])
    v = 0.5;
  h.node.inputs.mode = LoopMode::Overdub;
  for(int i = 0; i < 32; i++)
    h.run(64);
  h.reset_input();
  h.node.inputs.mode = LoopMode::Play;
  h.run(64);

  CHECK(h.node.layers.length == 2048);
  // The first pass plays again under each part of the dub.
  CHECK(h.node.layers.mix(0, 256) == Approx(0.75));
  CHECK(h.node.layers.mix(0, 3 * 512 + 256) == Approx(0.75));

  // Undo gives the loop its length back; redo the longer one.
  press(h.node.inputs.undo, h);
  CHECK(h.node.layers.length == 512);
  press(h.node.inputs.redo, h);
  CHECK(h.node.layers.length == 2048);

  // A dub that stops shorter than the loop does not change it.
  for(auto& v : h.in_buf[0])
    v = 0.125;
  h.node.inputs.mode = LoopMode::Overdub;
  for(int i = 0; i < 8; i++)
    h.run(64);
  h.reset_input();
  h.node.inputs.mode = LoopMode::Play;
  h.run(64);
  CHECK(h.node.layers.length == 2048);
}

TEST_CASE("Looper: an extending overdub stopped just past the end does not add a loop",
          "[fx][audio][looper][layers]")
{
  looper_harness h{1, 64};
  h.node.inputs.past_end.value = Nodes::Looper::PastEnd::Extend;
  h.node.inputs.start_from = Nodes::Looper::StartFrom::WhereItWas;
  h.node.inputs.late.value = 1; // 48 frames
  for(auto& v : h.in_buf[0])
    v = 0.25;
  h.node.inputs.mode = LoopMode::Record;
  for(int i = 0; i < 8; i++)
    h.run(64);
  h.node.inputs.mode = LoopMode::Overdub;
  for(int i = 0; i < 24; i++) // three lengths
    h.run(64);
  h.run(32);
  h.node.inputs.mode = LoopMode::Play;
  h.run(64);
  CHECK(h.node.layers.length == 1536);
}

TEST_CASE("Looper: following the tempo, a loop recorded at 120 lasts its bars at 60",
          "[fx][audio][looper][tempo]")
{
  for(auto mode : {Nodes::Looper::FollowTempo::Repitch, Nodes::Looper::FollowTempo::Timestretch})
  {
    // 1000 frames a second, a 500-frame loop recorded at 120 bpm.
    looper_harness h{1, 100, 1000};
    h.node.inputs.follow_tempo.value = mode;
    h.node.prepare_stretch(1000, 100);
    h.node.inputs.passthrough = Passthrough::None;
    h.fill_input(0, [](int64_t i) { return std::sin(i * 0.3); });
    h.node.inputs.mode = LoopMode::Record;
    for(int i = 0; i < 5; i++)
      h.run(100);
    h.node.inputs.mode = LoopMode::Play;
    h.run(100);
    REQUIRE(h.node.layers.length == 500);

    // At 60 the loop takes twice as long, from its next round: then four
    // ticks of 100 frames go 200 frames into it, not 400.
    h.tempo = 60.;
    for(int i = 0; i < 5; i++)
      h.run(100);
    const int64_t before = h.node.transport.playbackPos;
    for(int i = 0; i < 4; i++)
      h.run(100);
    INFO(int(mode));
    const int64_t moved = (h.node.transport.playbackPos - before + 500) % 500;
    CHECK(moved > 150);
    CHECK(moved < 260);
  }
}

TEST_CASE("Looper: an extending overdub stopped just past the end does not add a loop, "
          "on the grid and restarting",
          "[fx][audio][looper][layers][musical]")
{
  // The defaults: the loop is read from its start on every change, and the
  // stop waits for the grid -- a bar, which the overdub has just grown the
  // loop by.
  musical_harness h{4800};
  h.node.inputs.quantif.value = halp::grid_value::bars(1);
  h.node.inputs.postaction_bars.value = 0;
  h.node.inputs.late.value = 120;
  h.node.inputs.past_end.value = Nodes::Looper::PastEnd::Extend;

  h.node.inputs.mode = LoopMode::Stop;
  h.run_bars(1);
  h.fill_input(0, [](int64_t) { return 0.25; });
  h.node.inputs.mode = LoopMode::Record;
  h.run_bars(1);
  h.node.inputs.mode = LoopMode::Overdub;
  h.run_bars(2); // the loop grows on each of the two bar lines
  REQUIRE(h.node.layers.length == 3 * h.bar_samples());

  // 100 ms past the bar line the loop last grew on: the stop is late.
  h.run(h.mtick(h.buffer_size));
  h.node.inputs.mode = LoopMode::Play;
  h.run(h.mtick(h.buffer_size));

  CHECK(h.node.transport.actualMode == LoopMode::Play);
  CHECK(h.node.layers.length == 2 * h.bar_samples());
  CHECK(h.node.layers.top()->written == 2 * h.bar_samples());
  // Playback carries on where the loop would be had the stop been on time.
  CHECK(h.node.transport.playbackPos == 2 * h.buffer_size);
}

TEST_CASE("Looper: following the tempo, the position is the one heard",
          "[fx][audio][looper][tempo]")
{
  // The stretchers read ahead of what they output. The position is what is
  // heard: an overdub carrying on from playback starts there, not where the
  // stretcher has read up to.
  for(auto mode :
      {Nodes::Looper::FollowTempo::Repitch, Nodes::Looper::FollowTempo::RepitchFast,
       Nodes::Looper::FollowTempo::Timestretch})
  {
    INFO(int(mode));
    looper_harness h{1, 100, 1000};
    h.node.inputs.follow_tempo.value = mode;
    h.node.inputs.start_from = Nodes::Looper::StartFrom::WhereItWas;
    h.node.prepare_stretch(1000, 100);
    h.node.inputs.passthrough = Passthrough::None;
    h.fill_input(0, [](int64_t i) { return std::sin(i * 0.3); });
    h.node.inputs.mode = LoopMode::Record;
    for(int i = 0; i < 5; i++)
      h.run(100);
    REQUIRE(h.node.layers.length == 500);

    // At the tempo it was recorded at: a frame of the loop per frame.
    h.node.inputs.mode = LoopMode::Play;
    for(int i = 1; i <= 7; i++)
    {
      h.run(100);
      CHECK(h.node.transport.playbackPos == (100 * i) % 500);
    }

    h.reset_input();
    h.node.inputs.mode = LoopMode::Overdub;
    h.run(100);
    REQUIRE(h.node.layers.count == 2);
    CHECK(h.node.layers.top()->start == 200);
  }
}

TEST_CASE("Looper: following the tempo allocates nothing, with more channels than "
          "fit inline",
          "[fx][audio][looper][tempo]")
{
  for(auto mode :
      {Nodes::Looper::FollowTempo::Repitch, Nodes::Looper::FollowTempo::RepitchFast})
  {
    INFO(int(mode));
    looper_harness h{2, 64};
    h.node.layers.allocate(12, 2, 48000);
    h.node.inputs.follow_tempo.value = mode;
    h.node.prepare_stretch(48000, 64);
    h.fill_input(0, [](int64_t i) { return std::sin(i * 0.1); });
    h.fill_input(1, [](int64_t i) { return std::cos(i * 0.1); });
    h.node.inputs.mode = LoopMode::Record;
    for(int i = 0; i < 20; i++)
      h.run(64);

    int allocated = 0;
    {
      alloc_counter::scope counter;
      h.node.inputs.mode = LoopMode::Play;
      h.run(64); // the recording closes at its own tempo
      // From the next round, the loop is stretched for real.
      h.tempo = 90.;
      for(int i = 0; i < 40; i++)
        h.run(64);
      allocated = counter.allocations();
    }
    CHECK(allocated == 0);
  }
}

TEST_CASE("Looper: the two oldest passes merge over several ticks, heard as they were",
          "[fx][audio][looper][layers]")
{
  // 600000 frames of one channel: a quarter of a million are merged a tick,
  // so three ticks.
  looper_harness h{1, 4096};
  h.node.layers.allocate(1, 2, 700000);
  record_and_overdub(h, 600000, 0.25, 0.5);
  REQUIRE(h.node.layers.count == 2);

  for(auto& v : h.in_buf[0])
    v = 0.125;
  h.node.inputs.mode = LoopMode::Overdub;
  h.run(4096);
  // The new pass is heard at once, over the two being merged.
  CHECK(h.node.layers.count == 3);
  CHECK(h.node.layers.mix(0, 1000) == Approx(0.875));
  CHECK(h.node.layers.mix(0, 300000) == Approx(0.75));

  h.run(4096);
  CHECK(h.node.layers.count == 3);
  h.run(4096);
  CHECK(h.node.layers.count == 2);
  CHECK(h.node.layers.mix(0, 1000) == Approx(0.875));
  CHECK(h.node.layers.mix(0, 300000) == Approx(0.75));

  // Undo takes away the new pass and leaves the merged one.
  h.reset_input();
  h.node.inputs.mode = LoopMode::Play;
  h.run(4096);
  press(h.node.inputs.undo, h);
  CHECK(h.node.layers.count == 1);
  CHECK(h.node.layers.mix(0, 1000) == Approx(0.75));
}

TEST_CASE("Looper: a take is flattened a bounded amount per tick, by channels and passes",
          "[fx][audio][looper][takes]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  looper_harness h{4, 4096};
  h.node.layers.allocate(4, 8, 100000);
  h.node.export_audio = Nodes::Looper::export_buffer::make(4, 100000);
  auto writer = std::make_shared<Nodes::Looper::take_writer>();
  writer->folder = dir.path();
  writer->prefix = "t";
  writer->extension = "wav";
  h.node.writer = writer;
  h.node.worker.request = [&](Nodes::Looper::take_job job) {
    Looper::worker_t::work(std::move(job))(h.node);
  };
  auto files = [&] { return QDir{dir.path()}.entryList({"*.wav"}, QDir::Files).size(); };

  for(int c = 0; c < 4; c++)
    for(auto& v : h.in_buf[c])
      v = 0.25;
  h.node.inputs.mode = LoopMode::Record;
  for(int i = 0; i < 16; i++)
    h.run(4096);
  h.node.inputs.mode = LoopMode::Overdub;
  for(int i = 0; i < 16; i++)
    h.run(4096);
  REQUIRE(files() == 1);

  // Four channels of two passes, 65536 frames: half a million samples to add
  // up, which is two ticks of work.
  h.node.inputs.mode = LoopMode::Play;
  h.run(4096);
  CHECK(files() == 1);
  h.run(4096);
  CHECK(files() == 2);
}

namespace
{
//! A looper whose takes are written when the test says, as a worker thread
//! would at its own pace.
struct deferred_takes
{
  QTemporaryDir dir;
  std::vector<Nodes::Looper::take_job> jobs;

  explicit deferred_takes(looper_harness& h)
  {
    auto writer = std::make_shared<Nodes::Looper::take_writer>();
    writer->folder = dir.path();
    writer->prefix = "t";
    writer->extension = "wav";
    h.node.writer = writer;
    h.node.worker.request = [this](Nodes::Looper::take_job job) {
      jobs.push_back(std::move(job));
    };
    h.node.inputs.exported.value = Nodes::Looper::TakeFiles::Layers;
  }

  //! Writes what is queued, the looper flattening the next file of a take in
  //! the tick after the previous one is written.
  void finish(looper_harness& h)
  {
    for(int i = 0; i < 64; i++)
    {
      while(!jobs.empty())
      {
        auto job = std::move(jobs.front());
        jobs.erase(jobs.begin());
        Looper::worker_t::work(std::move(job))(h.node);
      }
      h.run(h.buffer_size);
    }
  }

  bool exists(const char* name) const { return QFileInfo::exists(dir.path() + "/" + name); }
  float sample(const char* name, int64_t frame) const
  {
    const auto wav = read_wav(QString(dir.path() + "/" + name).toStdString());
    return frame * wav.channels < int64_t(wav.samples.size())
               ? wav.samples[frame * wav.channels]
               : -1.f;
  }
};

void fill(looper_harness& h, double v)
{
  for(auto& x : h.in_buf[0])
    x = v;
}

void take_in(looper_harness& h, LoopMode mode, double v, int ticks)
{
  fill(h, v);
  h.node.inputs.mode = mode;
  for(int i = 0; i < ticks; i++)
    h.run(h.buffer_size);
  h.reset_input();
  h.node.inputs.mode = LoopMode::Play;
  h.run(h.buffer_size);
}
}

TEST_CASE("Looper: the files of a take are its passes as they were when it closed",
          "[fx][audio][looper][takes]")
{
  SECTION("an undo and a new overdub while it is written leave it as it was")
  {
    looper_harness h{1, 64};
    h.node.layers.allocate(1, 3, 48000);
    deferred_takes takes{h};
    take_in(h, LoopMode::Record, 0.25, 8);
    takes.finish(h);
    REQUIRE(takes.exists("t-take1-layer1.wav"));

    // The take of the overdub is being written...
    take_in(h, LoopMode::Overdub, 0.5, 8);
    REQUIRE(!takes.jobs.empty());
    // ... when the overdub is undone and another one made in its place.
    press(h.node.inputs.undo, h);
    take_in(h, LoopMode::Overdub, 0.125, 8);
    takes.finish(h);

    REQUIRE(takes.exists("t-take2-layer1.wav"));
    REQUIRE(takes.exists("t-take2-layer2.wav"));
    CHECK(takes.sample("t-take2-layer1.wav", 256) == Approx(0.25));
    CHECK(takes.sample("t-take2-layer2.wav", 256) == Approx(0.5));
    // And the take of the new overdub is the new overdub.
    REQUIRE(takes.exists("t-take3-layer2.wav"));
    CHECK(takes.sample("t-take3-layer2.wav", 256) == Approx(0.125));
  }

  SECTION("a pass that needs the buffers it reads stops it, unpublished")
  {
    looper_harness h{1, 64};
    h.node.layers.allocate(1, 2, 48000);
    deferred_takes takes{h};
    Nodes::Looper::view_message last{};
    h.node.send_message = [&](Nodes::Looper::view_message v) { last = v; };
    take_in(h, LoopMode::Record, 0.25, 8);
    takes.finish(h);

    take_in(h, LoopMode::Overdub, 0.5, 8);
    REQUIRE(!takes.jobs.empty());
    // Two more passes: past the buffers kept, the second one needs the ones
    // the take still reads.
    take_in(h, LoopMode::Overdub, 0.125, 8);
    take_in(h, LoopMode::Overdub, 0.0625, 8);
    takes.finish(h);

    // What was written of it is right, and what was not is not made up from
    // the passes that came after.
    if(takes.exists("t-take2-layer1.wav"))
      CHECK(takes.sample("t-take2-layer1.wav", 256) == Approx(0.25));
    if(takes.exists("t-take2-layer2.wav"))
      CHECK(takes.sample("t-take2-layer2.wav", 256) == Approx(0.5));
    // The take offered to send is a whole one.
    const auto paths = last.take_files();
    REQUIRE(!paths.empty());
    for(const auto& p : paths)
      CHECK(QFileInfo::exists(QString::fromStdString(p)));
    CHECK(QString::fromStdString(paths.back()).endsWith(
        QString("-layer%1.wav").arg(paths.size())));
  }
}

// ---------------------------------------------------------------------------
// Erasing a pass.
// ---------------------------------------------------------------------------

namespace
{
//! One more overdub pass of a constant over the whole loop, ending in Play.
void overdub_pass(looper_harness& h, int64_t len, double value)
{
  for(auto& v : h.in_buf[0])
    v = value;
  h.node.inputs.mode = LoopMode::Overdub;
  for(int64_t done = 0; done < len; done += h.buffer_size)
    h.run(std::min<int64_t>(h.buffer_size, len - done));
  h.reset_input();
  h.node.inputs.mode = LoopMode::Play;
  h.run(h.buffer_size);
}

//! Erases a pass with the ports, counting from one.
void erase_layer(looper_harness& h, int layer)
{
  h.node.inputs.layer.value = layer;
  press(h.node.inputs.erase_layer, h);
}
}

TEST_CASE("Looper: erasing the middle pass gives the passes under it back what it took",
          "[fx][audio][looper][layers][erase]")
{
  looper_harness h{1, 64};
  h.node.inputs.feedback.value = 0.5f;
  record_and_overdub(h, 512, 0.25, 0.5);
  h.node.inputs.feedback.value = 1.f;
  overdub_pass(h, 512, 0.125);
  REQUIRE(h.node.layers.count == 3);
  REQUIRE(h.node.layers.layers[0].gain == Approx(0.5));
  CHECK(h.node.layers.mix(0, 320) == Approx(0.25 * 0.5 + 0.5 + 0.125));

  erase_layer(h, 2);
  CHECK(h.node.layers.count == 2);
  CHECK(h.node.layers.total == 2);
  CHECK(h.node.layers.length == 512);
  CHECK(h.node.layers.layers[0].gain == Approx(1.));
  CHECK(h.node.layers.mix(0, 320) == Approx(0.25 + 0.125));

  // Undo goes on with the passes left.
  press(h.node.inputs.undo, h);
  CHECK(h.node.layers.count == 1);
  CHECK(h.node.layers.mix(0, 320) == Approx(0.25));
  press(h.node.inputs.redo, h);
  CHECK(h.node.layers.mix(0, 320) == Approx(0.375));
}

TEST_CASE("Looper: erasing a pass keeps the feedback of the ones over it",
          "[fx][audio][looper][layers][erase]")
{
  looper_harness h{1, 64};
  record_and_overdub(h, 512, 0.25, 0.5);
  h.node.inputs.feedback.value = 0.5f;
  overdub_pass(h, 512, 0.125);
  REQUIRE(h.node.layers.mix(0, 320) == Approx(0.25 * 0.5 + 0.5 * 0.5 + 0.125));

  // The third pass took half of both passes under it: the first keeps it.
  erase_layer(h, 2);
  CHECK(h.node.layers.mix(0, 320) == Approx(0.25 * 0.5 + 0.125));
  erase_layer(h, 2);
  CHECK(h.node.layers.mix(0, 320) == Approx(0.25));
}

TEST_CASE("Looper: erasing the pass being overdubbed is undoing it",
          "[fx][audio][looper][layers][erase]")
{
  looper_harness h{1, 64};
  record_and_overdub(h, 512, 0.25, 0.5);
  for(auto& v : h.in_buf[0])
    v = 1.;
  h.node.inputs.mode = LoopMode::Overdub;
  h.run(64);
  REQUIRE(h.node.layers.count == 3);

  h.node.inputs.layer.value = 3;
  h.node.inputs.erase_layer.value.emplace();
  h.run(64);
  h.node.inputs.erase_layer.value.reset();
  // The pass was dropped, and overdubbing carries on in a new one.
  CHECK(h.node.layers.count == 3);
  CHECK(h.node.layers.top()->written == 64);

  // A pass under it goes while overdubbing goes on.
  h.node.inputs.layer.value = 1;
  h.node.inputs.erase_layer.value.emplace();
  h.run(64);
  h.node.inputs.erase_layer.value.reset();
  CHECK(h.node.layers.count == 2);
  CHECK(h.node.transport.actualMode == LoopMode::Overdub);
  CHECK(h.node.layers.top()->written == 128);
  CHECK(h.node.layer_data(0, 0)[320] == Approx(0.5));
}

TEST_CASE("Looper: erasing the only pass empties the loop", "[fx][audio][looper][layers][erase]")
{
  looper_harness h{1, 64};
  h.node.inputs.passthrough = Passthrough::None;
  record_and_overdub(h, 512, 0.25, 0.5);
  press(h.node.inputs.undo, h);
  REQUIRE(h.node.layers.count == 1);

  erase_layer(h, 1);
  CHECK(h.node.layers.empty());
  CHECK(h.node.layers.length == 0);
  // Like clear, nothing is left to redo.
  press(h.node.inputs.redo, h);
  CHECK(h.node.layers.empty());
  h.reset_output();
  h.run(64);
  for(int i = 0; i < 64; i++)
    CHECK(h.out_buf[0][i] == Approx(0.));

  // A pass that is not there is left alone.
  record_and_overdub(h, 512, 0.25, 0.5);
  erase_layer(h, 5);
  CHECK(h.node.layers.count == 2);
}

TEST_CASE("Looper: erasing a pass keeps what redo brings back",
          "[fx][audio][looper][layers][erase]")
{
  looper_harness h{1, 64};
  record_and_overdub(h, 512, 0.25, 0.5);
  overdub_pass(h, 512, 0.125);
  press(h.node.inputs.undo, h);
  REQUIRE(h.node.layers.count == 2);
  REQUIRE(h.node.layers.total == 3);

  erase_layer(h, 1);
  CHECK(h.node.layers.count == 1);
  CHECK(h.node.layers.total == 2);
  CHECK(h.node.layers.mix(0, 320) == Approx(0.5));
  press(h.node.inputs.redo, h);
  CHECK(h.node.layers.count == 2);
  CHECK(h.node.layers.mix(0, 320) == Approx(0.625));
}

TEST_CASE("Looper: erasing an extending pass keeps the loop it made",
          "[fx][audio][looper][layers][erase]")
{
  looper_harness h{1, 64};
  record_and_overdub(h, 512, 0.25, 0.5);
  h.node.inputs.past_end = Nodes::Looper::PastEnd::Extend;
  overdub_pass(h, 1024, 0.125);
  const int64_t length = h.node.layers.length;
  REQUIRE(length > 512);
  REQUIRE(h.node.layers.count == 3);

  erase_layer(h, 3);
  CHECK(h.node.layers.length == length);
  // The passes under it play again over the part it added.
  CHECK(h.node.layers.mix(0, 512 + 320) == Approx(0.75));
}

TEST_CASE("Looper: the view asks for a pass to be erased, a changed loop is left alone",
          "[fx][audio][looper][layers][erase]")
{
  looper_harness h{1, 64};
  record_and_overdub(h, 512, 0.25, 0.5);
  overdub_pass(h, 512, 0.125);

  h.node.process_message(Nodes::Looper::erase_request{.pass = 0, .layers = 2});
  h.run(64);
  CHECK(h.node.layers.count == 3);

  h.node.process_message(Nodes::Looper::erase_request{.pass = 0, .layers = 3});
  h.run(64);
  CHECK(h.node.layers.count == 2);
  CHECK(h.node.layers.mix(0, 320) == Approx(0.625));
}

TEST_CASE("Looper: the overview of the passes follows an erase",
          "[fx][audio][looper][layers][erase]")
{
  looper_harness h{1, 64};
  Nodes::Looper::view_message last{};
  h.node.send_message = [&](Nodes::Looper::view_message v) { last = v; };
  record_and_overdub(h, 512, 0.25, 0.5);
  overdub_pass(h, 512, 0.125);
  for(int i = 0; i < 30; i++)
    h.run(64);
  REQUIRE(last.lanes == 3);

  erase_layer(h, 2);
  for(int i = 0; i < 30; i++)
    h.run(64);
  CHECK(last.layers == 2);
  CHECK(last.lanes == 2);
  constexpr int bins = Nodes::Looper::view_message::bins;
  CHECK(last.peaks[bins / 2] == Approx(0.25).margin(0.01));
  CHECK(last.peaks[bins + bins / 2] == Approx(0.125).margin(0.01));
}

TEST_CASE("Looper: erasing a pass while the two oldest merge",
          "[fx][audio][looper][layers][erase]")
{
  for(int layer : {1, 2, 3})
  {
    INFO("erasing layer " << layer);
    looper_harness h{1, 4096};
    h.node.layers.allocate(1, 2, 700000);
    record_and_overdub(h, 600000, 0.25, 0.5);
    for(auto& v : h.in_buf[0])
      v = 0.125;
    h.node.inputs.mode = LoopMode::Overdub;
    h.run(4096);
    REQUIRE(h.node.layers.merging.active);
    REQUIRE(h.node.layers.count == 3);

    h.node.inputs.layer.value = layer;
    h.node.inputs.erase_layer.value.emplace();
    h.run(4096);
    h.node.inputs.erase_layer.value.reset();
    for(int i = 0; i < 4; i++)
      h.run(4096);
    CHECK(!h.node.layers.merging.active);
    CHECK(h.node.layers.count == 2);
    const double expected = layer == 1 ? 0.5 : layer == 2 ? 0.25 : 0.75;
    CHECK(h.node.layers.mix(0, 300000) == Approx(expected));
    // Every pass heard has a buffer of its own.
    for(int i = 0; i < h.node.layers.count; i++)
      for(int j = i + 1; j < h.node.layers.count; j++)
        CHECK(h.node.layers.layers[i].slot != h.node.layers.layers[j].slot);
  }
}

TEST_CASE("Looper: erasing a pass allocates nothing", "[fx][audio][looper][layers][erase]")
{
  looper_harness h{2, 64};
  h.fill_input(0, [](int64_t i) { return std::sin(i * 0.1); });
  int allocated = 0;
  {
    alloc_counter::scope counter;
    h.node.inputs.mode = LoopMode::Record;
    for(int i = 0; i < 8; i++)
      h.run(64);
    h.node.inputs.mode = LoopMode::Overdub;
    for(int i = 0; i < 40; i++)
      h.run(64);
    h.node.inputs.mode = LoopMode::Play;
    h.run(64);
    h.node.inputs.mode = LoopMode::Overdub;
    for(int i = 0; i < 10; i++)
      h.run(64);
    erase_layer(h, 2);
    h.node.process_message(Nodes::Looper::erase_request{.pass = 0, .layers = h.node.layers.count});
    h.run(64);
    erase_layer(h, 1);
    allocated = counter.allocations();
  }
  CHECK(allocated == 0);
}

// ---------------------------------------------------------------------------
// Reverse, speed, a pass per cycle, and playing in phase with the timeline.
// ---------------------------------------------------------------------------

namespace
{
//! Records `len` frames of what `f` gives for each frame, then asks for Play.
void record_with(looper_harness& h, int64_t len, double (*f)(int64_t))
{
  h.node.inputs.mode = LoopMode::Record;
  for(int64_t done = 0; done < len; done += h.buffer_size)
  {
    const int64_t n = std::min<int64_t>(h.buffer_size, len - done);
    for(int64_t i = 0; i < n; i++)
      h.in_buf[0][i] = f(done + i);
    h.run(n);
  }
  h.reset_input();
  h.node.inputs.mode = LoopMode::Play;
}

//! Runs `frames` frames, the input of frame i (from the first of them) being
//! f(i), and gives back what came out.
std::vector<double> run_with(looper_harness& h, int64_t frames, double (*f)(int64_t))
{
  std::vector<double> out;
  for(int64_t done = 0; done < frames; done += h.buffer_size)
  {
    const int64_t n = std::min<int64_t>(h.buffer_size, frames - done);
    for(int64_t i = 0; i < n; i++)
      h.in_buf[0][i] = f ? f(done + i) : 0.;
    h.reset_output();
    h.run(n);
    out.insert(out.end(), h.out_buf[0].begin(), h.out_buf[0].begin() + n);
  }
  h.reset_input();
  return out;
}

//! Times the signal goes from below zero to above it over [from; to[.
int rising_crossings(const std::vector<double>& v, std::size_t from, std::size_t to)
{
  int n = 0;
  for(std::size_t i = from + 1; i < to && i < v.size(); i++)
    n += v[i - 1] < 0. && v[i] >= 0.;
  return n;
}

double ramp(int64_t i)
{
  return 0.001 * double(i % 512 + 1);
}
double sine32(int64_t i)
{
  return std::sin(2. * M_PI * (double(i) + 0.25) / 32.);
}
double sine64(int64_t i)
{
  return std::sin(2. * M_PI * (double(i) + 0.25) / 64.);
}
double silent(int64_t)
{
  return 0.;
}
}

TEST_CASE("Looper: reversed, the loop plays backwards from where the playhead is",
          "[fx][audio][looper][reverse]")
{
  looper_harness h{1, 64};
  h.node.inputs.passthrough = Passthrough::None;
  record_with(h, 1024, ramp);
  h.node.inputs.reverse.value = true;

  // The recording closes and the loop is read from its start: backwards,
  // that is its last frame.
  h.reset_output();
  h.run(64);
  for(int j = 0; j < 64; j++)
    CHECK(h.out_buf[0][j] == Approx(h.loop()[1023 - j]));
  CHECK(h.node.transport.playbackPos == 1024 - 64);
  CHECK(h.node.outputs.phase.value == Approx(960. / 1024.));

  h.reset_output();
  h.run(64);
  for(int j = 0; j < 64; j++)
    CHECK(h.out_buf[0][j] == Approx(h.loop()[959 - j]));
  CHECK(h.node.transport.playbackPos == 896);

  // Forwards again, from there.
  h.node.inputs.reverse.value = false;
  h.reset_output();
  h.run(64);
  for(int j = 0; j < 64; j++)
    CHECK(h.out_buf[0][j] == Approx(h.loop()[896 + j]));
  CHECK(h.node.transport.playbackPos == 960);
}

TEST_CASE("Looper: an overdub over the reversed loop sounds reversed once it plays "
          "forwards",
          "[fx][audio][looper][reverse]")
{
  looper_harness h{1, 64};
  h.node.inputs.passthrough = Passthrough::None;
  record_with(h, 1024, [](int64_t) { return 0.25; });
  h.run(64);

  h.node.inputs.reverse.value = true;
  h.node.inputs.mode = LoopMode::Overdub;
  // The overdub starts from the loop's start, going backwards: its frame j
  // is written at position 1023 - j.
  run_with(h, 1024, ramp);
  REQUIRE(h.node.layers.count == 2);
  const auto& top = *h.node.layers.top();
  CHECK(top.written == 1024);
  CHECK(top.backwards);
  for(int64_t j = 128; j < 1024; j++)
    CHECK(h.node.layer_data(1, 0)[1023 - j] == Approx(ramp(j)));

  h.node.inputs.reverse.value = false;
  h.node.inputs.mode = LoopMode::Play;
  const auto out = run_with(h, 1024, nullptr);
  for(int64_t k = 200; k < 800; k++)
    CHECK(out[k] == Approx(0.25 + ramp(1023 - k)));
}

TEST_CASE("Looper: at half speed a loop's sine is an octave down, at double speed up",
          "[fx][audio][looper][speed]")
{
  looper_harness h{1, 64};
  h.node.inputs.passthrough = Passthrough::None;
  record_with(h, 1024, sine32);

  SECTION("x1/2")
  {
    h.node.inputs.speed.value = 0.5f;
    const auto out = run_with(h, 2048, nullptr);
    // The whole loop takes twice as long.
    CHECK(h.node.transport.playbackPos == 0);
    // Away from the faded edges, the sine's period is 64 frames.
    for(std::size_t j = 256; j < 1792; j++)
      CHECK(out[j] == Approx(std::sin(2. * M_PI * double(j) / 64.)).margin(0.01));
    CHECK(rising_crossings(out, 250, 1790) == 24);
  }
  SECTION("x2")
  {
    h.node.inputs.speed.value = 2.f;
    const auto out = run_with(h, 256, nullptr);
    CHECK(h.node.transport.playbackPos == 512);
    // Two positions a frame, averaged: a period of 16 frames.
    for(std::size_t j = 64; j < 256; j++)
      CHECK(out[j] == Approx(std::sin(2. * M_PI * (2. * j + 0.75) / 32.)).margin(0.02));
    CHECK(rising_crossings(out, 60, 250) == 12);
  }
  SECTION("x1/2 backwards: the frames between two positions are interpolated")
  {
    h.node.inputs.speed.value = 0.5f;
    h.node.inputs.reverse.value = true;
    run_with(h, 640, nullptr);
    CHECK(h.node.transport.playbackPos == 1024 - 320);
    CHECK(h.node.outputs.phase.value == Approx(704. / 1024.));
  }
}

TEST_CASE("Looper: an overdub at another speed writes where the playhead goes",
          "[fx][audio][looper][speed]")
{
  looper_harness h{1, 64};
  h.node.inputs.passthrough = Passthrough::None;
  record_with(h, 1024, silent);
  h.run(64);

  SECTION("played at x1/2, it is an octave up at x1")
  {
    h.node.inputs.speed.value = 0.5f;
    h.node.inputs.mode = LoopMode::Overdub;
    run_with(h, 2048, sine64);
    REQUIRE(h.node.layers.count == 2);
    CHECK(h.node.layers.top()->written == 1024);
    // Two frames of input for each position, averaged.
    const float* d = h.node.layer_data(1, 0);
    for(int64_t k = 128; k < 1000; k++)
      CHECK(d[k] == Approx(std::cos(M_PI / 64.) * std::sin(2. * M_PI * (k + 0.375) / 32.))
                        .margin(1e-3));

    h.node.inputs.speed.value = 1.f;
    h.node.inputs.mode = LoopMode::Play;
    const auto out = run_with(h, 1024, nullptr);
    CHECK(rising_crossings(out, 120, 890) == 24);
  }
  SECTION("played at x2, it is an octave down at x1")
  {
    h.node.inputs.speed.value = 2.f;
    h.node.inputs.mode = LoopMode::Overdub;
    run_with(h, 512, sine32);
    REQUIRE(h.node.layers.count == 2);
    CHECK(h.node.layers.top()->written == 1024);
    // Two positions for each frame, the input interpolated between frames.
    const float* d = h.node.layer_data(1, 0);
    for(int64_t k = 256; k < 1000; k++)
      CHECK(d[k] == Approx(std::sin(2. * M_PI * ((k - 1) / 2. + 0.25) / 32.)).margin(0.01));

    h.node.inputs.speed.value = 1.f;
    h.node.inputs.mode = LoopMode::Play;
    const auto out = run_with(h, 1024, nullptr);
    CHECK(rising_crossings(out, 250, 1000) == 12);
  }
}

TEST_CASE("Looper: speed and reverse leave the grid and the loop's length alone",
          "[fx][audio][looper][speed][musical]")
{
  // A recording of four bars at x1/2: the recording is the loop, at x1, and
  // the post-action takes over after its bars.
  musical_harness h{4800};
  h.node.inputs.quantif.value = halp::grid_value::bars(1);
  h.node.inputs.postaction_bars.value = 1;
  h.node.inputs.speed.value = 0.5f;
  h.node.inputs.reverse.value = true;
  h.fill_input(0, [](int64_t) { return 0.25; });
  h.node.inputs.mode = LoopMode::Stop;
  h.run_bars(1);
  h.node.inputs.mode = LoopMode::Record;
  h.run_bars(1.5);
  CHECK(h.node.transport.actualMode == LoopMode::Play);
  CHECK(h.node.layers.length == h.bar_samples());
}

TEST_CASE("Looper: a pass per cycle makes a pass of each time an overdub goes round",
          "[fx][audio][looper][comp]")
{
  looper_harness h{1, 64};
  h.node.inputs.passthrough = Passthrough::None;
  h.node.inputs.passes.value = Nodes::Looper::Passes::PerCycle;
  record_with(h, 512, [](int64_t) { return 0.25; });
  h.run(64);

  h.node.inputs.mode = LoopMode::Overdub;
  run_with(h, 1536, [](int64_t i) { return 0.1 * double(i / 512 + 1); });
  h.node.inputs.mode = LoopMode::Play;
  h.run(64);

  // The recording, then a pass for each of the three rounds, and none for
  // the round the overdub stopped at the start of.
  REQUIRE(h.node.layers.count == 4);
  CHECK(h.node.layer_data(1, 0)[256] == Approx(0.1));
  CHECK(h.node.layer_data(2, 0)[256] == Approx(0.2));
  CHECK(h.node.layer_data(3, 0)[256] == Approx(0.3));
  // Each round is a pass as any, faded in and out.
  CHECK(h.node.layer_data(2, 0)[0] == Approx(0.));
  CHECK(h.node.layer_data(2, 0)[200] == Approx(0.2));
  CHECK(h.node.layers.mix(0, 256) == Approx(0.85));

  SECTION("muting a pass takes it out of what is heard, and keeps it")
  {
    h.node.inputs.layer.value = 3;
    press(h.node.inputs.mute_layer, h);
    CHECK(h.node.layers.layers[2].muted);
    CHECK(h.node.layers.count == 4);
    CHECK(h.node.layers.mix(0, 256) == Approx(0.65));
    h.reset_output();
    h.run(64);
    CHECK(h.node.transport.playbackPos == 192);
    for(int j = 0; j < 64; j++)
      CHECK(h.out_buf[0][j] == Approx(h.node.layers.mix(0, 128 + j)));
    // The m of the lane says the same.
    Nodes::Looper::view_message last{};
    h.node.send_message = [&](Nodes::Looper::view_message v) { last = v; };
    for(int i = 0; i < 30; i++)
      h.run(64);
    CHECK(last.muted[2] == 1);
    CHECK(last.muted[1] == 0);

    press(h.node.inputs.mute_layer, h);
    CHECK(!h.node.layers.layers[2].muted);
    CHECK(h.node.layers.mix(0, 256) == Approx(0.85));
  }
  SECTION("the view mutes a pass of the loop it showed")
  {
    h.node.process_message(
        Nodes::Looper::erase_request{.pass = 1, .layers = 4, .mute = true});
    h.run(64);
    CHECK(h.node.layers.layers[1].muted);
    CHECK(h.node.layers.mix(0, 256) == Approx(0.75));
    h.node.process_message(
        Nodes::Looper::erase_request{.pass = 2, .layers = 3, .mute = true});
    h.run(64);
    CHECK(!h.node.layers.layers[2].muted);
  }
  SECTION("each pass is undone and erased on its own")
  {
    press(h.node.inputs.undo, h);
    CHECK(h.node.layers.count == 3);
    CHECK(h.node.layers.mix(0, 256) == Approx(0.55));
    erase_layer(h, 2);
    CHECK(h.node.layers.count == 2);
    CHECK(h.node.layers.mix(0, 256) == Approx(0.45));
  }
}

TEST_CASE("Looper: a muted pass takes nothing from the passes under it",
          "[fx][audio][looper][comp]")
{
  looper_harness h{1, 64};
  h.node.inputs.passthrough = Passthrough::None;
  h.node.inputs.passes.value = Nodes::Looper::Passes::PerCycle;
  h.node.inputs.feedback.value = 0.5f;
  record_with(h, 512, [](int64_t) { return 0.25; });
  h.run(64);
  h.node.inputs.mode = LoopMode::Overdub;
  run_with(h, 1536, [](int64_t i) { return 0.1 * double(i / 512 + 1); });
  h.node.inputs.mode = LoopMode::Play;
  h.run(64);
  REQUIRE(h.node.layers.count == 4);

  // As one overdub of three rounds: each round fades what is under it.
  CHECK(h.node.layers.mix(0, 256) == Approx(0.25 / 8 + 0.1 / 4 + 0.2 / 2 + 0.3));

  h.node.inputs.layer.value = 3;
  press(h.node.inputs.mute_layer, h);
  CHECK(h.node.layers.mix(0, 256) == Approx(0.25 / 4 + 0.1 / 2 + 0.3));
}

TEST_CASE("Looper: a take of passes per cycle has a file for each, the muted ones out "
          "of its mix",
          "[fx][audio][looper][comp][takes]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  looper_harness h{1, 64};
  auto writer = std::make_shared<Nodes::Looper::take_writer>();
  writer->folder = dir.path();
  writer->prefix = "t";
  writer->extension = "wav";
  h.node.writer = writer;
  h.node.worker.request = [&](Nodes::Looper::take_job job) {
    Looper::worker_t::work(std::move(job))(h.node);
  };
  h.node.inputs.exported.value = Nodes::Looper::TakeFiles::Both;
  h.node.inputs.passthrough = Passthrough::None;
  h.node.inputs.passes.value = Nodes::Looper::Passes::PerCycle;

  record_with(h, 512, [](int64_t) { return 0.25; });
  h.run(64);
  h.node.inputs.mode = LoopMode::Overdub;
  run_with(h, 1024, [](int64_t i) { return 0.1 * double(i / 512 + 1); });
  // The second round is muted while the third is played.
  h.node.inputs.layer.value = 3;
  h.node.inputs.mute_layer.value.emplace();
  run_with(h, 64, [](int64_t) { return 0.3; });
  h.node.inputs.mute_layer.value.reset();
  run_with(h, 448, [](int64_t) { return 0.3; });
  h.node.inputs.mode = LoopMode::Play;
  for(int i = 0; i < 10; i++)
    h.run(64);

  // A take each time a round's pass closes, the recording's first: the last
  // one has every pass.
  const QString last = dir.path() + "/t-take4";
  REQUIRE(QFileInfo::exists(last + ".wav"));
  CHECK(!QFileInfo::exists(dir.path() + "/t-take5.wav"));
  for(int k = 1; k <= 4; k++)
    CHECK(QFileInfo::exists(last + QString("-layer%1.wav").arg(k)));
  const auto mix = read_wav(QString(last + ".wav").toStdString());
  REQUIRE(mix.samples.size() == 512 * std::size_t(mix.channels));
  CHECK(mix.samples[256 * mix.channels] == Approx(0.25 + 0.1 + 0.3));
  const auto muted = read_wav(QString(last + "-layer3.wav").toStdString());
  REQUIRE(muted.samples.size() == 512 * std::size_t(muted.channels));
  CHECK(muted.samples[256 * muted.channels] == Approx(0.2));
}

TEST_CASE("Looper: in phase, a loop played again is where it would be had it kept "
          "playing",
          "[fx][audio][looper][phase]")
{
  // 1000 frames a second, a loop of a second.
  looper_harness h{1, 100, 1000};
  h.node.inputs.passthrough = Passthrough::None;
  h.node.inputs.start_from = Nodes::Looper::StartFrom::InPhase;
  record_with(h, 1000, ramp);
  for(int i = 0; i < 3; i++)
    h.run(100);
  REQUIRE(h.node.transport.playbackPos == 300);

  // Stopped for half a second: it comes back half a loop on.
  h.node.inputs.mode = LoopMode::Stop;
  for(int i = 0; i < 5; i++)
    h.run(100);
  h.node.inputs.mode = LoopMode::Play;
  h.reset_output();
  h.run(100);
  for(int j = 0; j < 100; j++)
    CHECK(h.out_buf[0][j] == Approx(h.loop()[800 + j]));
  CHECK(h.node.transport.playbackPos == 900);

  SECTION("the score stopped and started again elsewhere")
  {
    h.node.stop();
    h.now = 3250;
    h.run(100);
    CHECK(h.node.transport.playbackPos == 350);
  }
  SECTION("the score jumps while the loop plays")
  {
    h.now += 120;
    h.run(100);
    CHECK(h.node.transport.playbackPos == 120);
    // Played on, it does not jump.
    h.run(100);
    CHECK(h.node.transport.playbackPos == 220);
  }
  SECTION("backwards and at double speed, it is where it would be going that way")
  {
    h.node.inputs.mode = LoopMode::Stop;
    h.run(100);
    h.node.inputs.reverse.value = true;
    h.node.inputs.speed.value = 2.f;
    h.node.inputs.mode = LoopMode::Play;
    // A second after the loop's start, at x2: two rounds exactly, then a
    // buffer goes over 200 positions, backwards.
    h.run(100);
    CHECK(h.node.transport.playbackPos == 1000 - 200);
  }
  SECTION("Recording carries on from where it stopped")
  {
    h.node.inputs.start_from = Nodes::Looper::StartFrom::WhereItWas;
    h.node.inputs.mode = LoopMode::Stop;
    for(int i = 0; i < 3; i++)
      h.run(100);
    h.node.inputs.mode = LoopMode::Play;
    h.run(100);
    CHECK(h.node.transport.playbackPos == 0);
  }
}

TEST_CASE("Looper: in phase and following the tempo, a loop keeps to the bars across "
          "a tempo change",
          "[fx][audio][looper][phase][tempo]")
{
  musical_harness h{4800};
  h.accumulate = true;
  h.node.inputs.quantif.value = halp::grid_value{};
  h.node.inputs.postaction_bars.value = 0;
  h.node.inputs.start_from = Nodes::Looper::StartFrom::InPhase;
  h.node.inputs.follow_tempo.value = Nodes::Looper::FollowTempo::Repitch;
  h.node.prepare_stretch(48000, 4800);
  h.node.inputs.passthrough = Passthrough::None;
  h.fill_input(0, [](int64_t i) { return std::sin(i * 0.3); });

  h.node.inputs.mode = LoopMode::Stop;
  h.run_bars(1);
  h.node.inputs.mode = LoopMode::Record;
  h.run_bars(1);
  h.node.inputs.mode = LoopMode::Play;
  h.run_bars(0.5);
  REQUIRE(h.node.layers.length == h.bar_samples());

  // Twice as slow, and stopped for three quarters of a bar of it.
  h.tempo = 60.;
  h.node.inputs.mode = LoopMode::Stop;
  for(int i = 0; i < 30; i++)
    h.run(h.mtick(h.buffer_size));
  const double quarters = h.position;
  h.node.inputs.mode = LoopMode::Play;
  h.run(h.mtick(h.buffer_size));

  // A bar of the loop is four quarters, wherever the tempo went: the loop
  // starts where the bar it was recorded on puts it, then plays a buffer at
  // half its speed.
  const double into = std::fmod(quarters - 8., 4.) / 4.;
  const auto expected = int64_t(into * double(h.node.layers.length)) + h.buffer_size / 2;
  CHECK(std::abs(h.node.transport.playbackPos - expected) <= 1);
}

TEST_CASE("Looper: reverse, speed, passes per cycle and mutes allocate nothing",
          "[fx][audio][looper][reverse][speed][comp]")
{
  looper_harness h{2, 64};
  h.fill_input(0, [](int64_t i) { return std::sin(i * 0.1); });
  h.fill_input(1, [](int64_t i) { return std::cos(i * 0.1); });
  h.node.inputs.passes.value = Nodes::Looper::Passes::PerCycle;
  h.node.inputs.start_from = Nodes::Looper::StartFrom::InPhase;
  int allocated = 0;
  {
    alloc_counter::scope counter;
    h.node.inputs.mode = LoopMode::Record;
    for(int i = 0; i < 8; i++)
      h.run(64);
    h.node.inputs.reverse.value = true;
    h.node.inputs.mode = LoopMode::Overdub;
    for(int i = 0; i < 20; i++)
      h.run(64);
    h.node.inputs.speed.value = 0.5f;
    for(int i = 0; i < 20; i++)
      h.run(64);
    h.node.inputs.speed.value = 2.f;
    h.node.inputs.reverse.value = false;
    for(int i = 0; i < 20; i++)
      h.run(64);
    h.node.inputs.mode = LoopMode::Play;
    for(int i = 0; i < 10; i++)
      h.run(64);
    h.node.inputs.layer.value = 2;
    press(h.node.inputs.mute_layer, h);
    h.node.inputs.mode = LoopMode::Stop;
    h.run(64);
    h.node.inputs.mode = LoopMode::Play;
    for(int i = 0; i < 10; i++)
      h.run(64);
    allocated = counter.allocations();
  }
  CHECK(allocated == 0);
  CHECK(h.node.layers.count > 4);
}

namespace
{
//! The looper of the switch scenarios: a quarter-note grid, ticks of a fifth
//! of a beat, every frame of input at 0.5 unless `ramp` is set, when frame t
//! of the timeline is (t % 100000) / 100000: each frame recorded can be told.
struct switches_harness : musical_harness
{
  static constexpr int64_t beat = 24000;
  static constexpr int64_t frames = 4800;
  std::string_view shown{};
  int shown_count{};
  bool ramp{};

  switches_harness()
      : musical_harness{int(frames)}
  {
    node.inputs.quantif.value = halp::grid_value::note(0.25);
    node.inputs.postaction_bars.value = 0;
    // The looper's own defaults.
    node.inputs.start_from = decltype(node.inputs.start_from){}.value;
    node.inputs.at_max_layers = decltype(node.inputs.at_max_layers){}.value;
    node.inputs.passthrough = Passthrough::None;
    node.inputs.mode = LoopMode::Stop;
    node.inputs.mode.update_controller = [this](std::string_view s) {
      shown = s;
      shown_count++;
    };
    fill_input(0, [](int64_t) { return 0.5; });
    for(int i = 0; i < 5; i++)
      tick_once();
  }

  static double ramp_at(int64_t t) { return double(t % 100000) / 100000.; }

  void tick_once()
  {
    if(ramp)
      for(int64_t i = 0; i < frames; i++)
        in_buf[0][i] = ramp_at(now + i);
    run(mtick(frames));
  }
  int passes() const { return node.layers.count; }
  bool muted(int k) const { return node.layers.layers[k].muted; }
  int64_t length() const { return node.layers.length; }
  int64_t position() const
  {
    return node.transport.playbackPos % std::max<int64_t>(1, node.layers.length);
  }
  //! What layer k holds where it was written, summed.
  double signature(int k) const
  {
    const auto& l = node.layers.layers[k];
    const int64_t len = node.layers.length;
    const float* d = node.layer_data(k, 0);
    double sum = 0.;
    for(int64_t i = 0; i < len; i++)
      if(node.layers.valid(l, i))
        sum += std::abs(d[node.layers.index(l, i)]);
    return sum;
  }
  void set_max_layers(int n)
  {
    node.inputs.max_layers.value = n;
    node.layers.allocate(node.layers.channels, n, node.layers.capacity);
  }
  void enable_capture()
  {
    node.inputs.keep.value = 10;
    node.allocate_ring(node.layers.channels, 10 * 48000);
  }
  //! Merges that take several ticks: a merge goes through as many samples a
  //! tick whatever the channels, and there are sixteen.
  void slow_merges()
  {
    node.inputs.channels.value = 16;
    node.layers.allocate(16, node.layers.slots, 8 * beat);
    node.export_audio = Nodes::Looper::export_buffer::make(16, 8 * beat);
  }
};
}

#include "LooperSwitchesScenarios.hpp"

TEST_CASE("Looper switches: recording a loop", "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::record_a_loop(h);
}

TEST_CASE("Looper switches: a recording into an overdub, by After record or by Overdub "
          "on an empty loop",
          "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::record_then_overdub(h);
}

TEST_CASE("Looper switches: play and stop", "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::play_and_stop(h);
}

TEST_CASE("Looper switches: overdub", "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::overdub(h);
}

TEST_CASE("Looper switches: a new layer while overdubbing, without a jump",
          "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::new_layer(h);
}

TEST_CASE("Looper switches: Retrigger", "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::retrigger(h);
}

TEST_CASE("Looper switches: Start from", "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::start_from(h);
}

TEST_CASE("Looper switches: Undo and Redo", "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::undo_redo(h);
}

TEST_CASE("Looper switches: Mute layer and Erase layer, the newest or a stepped back one",
          "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::mute_and_erase(h);
}

TEST_CASE("Looper switches: Solo layer", "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::solo(h);
}

TEST_CASE("Looper switches: Toggle reverse, Half speed, Double speed",
          "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::reverse_and_speed(h);
}

TEST_CASE("Looper switches: At max layers", "[fx][audio][looper][switches][layers]")
{
  switches_harness h;
  looper_switches::at_max_layers(h);
}

TEST_CASE("Looper switches: Capture shows Play in Loop", "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::capture_shows_play(h);
}

TEST_CASE("Looper switches: Undo or Clear while recording starts it again",
          "[fx][audio][looper][switches]")
{
  for(const bool clear : {false, true})
  {
    INFO(clear);
    switches_harness h;
    looper_switches::undo_while_recording(h, clear);
  }
}

TEST_CASE("Looper switches: presses before the grid queue", "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::presses_queue(h);
}

TEST_CASE("Looper switches: a cable on Loop asks once", "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::cable_on_loop(h);
}

TEST_CASE("Looper switches: the lanes' m and x act on the grid",
          "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::lanes_on_the_grid(h);
}

TEST_CASE("Looper switches: Per cycle with Extend", "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::per_cycle_extend(h);
}

TEST_CASE("Looper switches: Layer follows the two oldest layers merging",
          "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::layer_follows_merges(h);
}

TEST_CASE("Looper switches: a layer selected among the two merging names the merged one",
          "[fx][audio][looper][switches]")
{
  switches_harness h;
  auto& in = h.node.inputs;
  in.at_max_layers = Nodes::Looper::AtMaxLayers::MergeOldest;
  h.slow_merges();
  looper_switches::three_distinct_layers(h);
  const double s0 = h.signature(0), s1 = h.signature(1);
  looper_switches::at_beat(h, in.overdub);
  REQUIRE(h.node.layers.merging.active);
  // The newer of the two.
  in.layer.value = 2;
  h.tick_once();
  REQUIRE(h.node.layers.merging.active);
  looper_switches::beats(h, 2);
  REQUIRE(!h.node.layers.merging.active);
  REQUIRE(h.passes() == 3);
  CHECK(in.layer.value == 1);
  CHECK(looper_switches::same(h.signature(0), s0 + s1));
}

TEST_CASE("Looper switches: a late stop just after a cycle cut leaves no empty layer",
          "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::late_stop_after_cycle_cut(h);
}

TEST_CASE("Looper switches: undo brings back the layer a pass replaced",
          "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::replace_muted_undo(h);
}

TEST_CASE("Looper switches: the score stopping closes the layer",
          "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::stop_closes_layer(h);
}

TEST_CASE("Looper switches: Stop at", "[fx][audio][looper][switches]")
{
  switches_harness h;
  auto& in = h.node.inputs;
  looper_switches::record_loop(h);
  // A beat into the loop, and a tick.
  looper_switches::beats(h, 1);
  h.tick_once();
  REQUIRE(h.position() == switches_harness::beat + 2 * switches_harness::frames);

  SECTION("now")
  {
    in.stop_at.value = Nodes::Looper::StopAt::Now;
    looper_switches::press(h, in.stop);
    CHECK(looper_switches::mode(h) == LoopMode::Stop);
  }
  SECTION("at the loop's end, through Loop")
  {
    in.stop_at.value = Nodes::Looper::StopAt::LoopEnd;
    in.mode = LoopMode::Stop;
    looper_switches::next_beat(h);
    CHECK(looper_switches::mode(h) == LoopMode::Play);
    int64_t before = h.position();
    for(int i = 0; i < 100 && looper_switches::mode(h) == LoopMode::Play; i++)
    {
      before = h.position();
      h.tick_once();
    }
    CHECK(looper_switches::mode(h) == LoopMode::Stop);
    CHECK(before == 0);
  }
}

TEST_CASE("Looper switches: a recording ends into an overdub on the frame it ends on",
          "[fx][audio][looper][switches]")
{
  for(const bool on_empty : {false, true})
  {
    switches_harness h;
    h.ramp = true;
    auto& in = h.node.inputs;
    if(on_empty)
    {
      looper_switches::press_off_beat(h, in.overdub);
    }
    else
    {
      in.after_record.value = Nodes::Looper::AfterRecord::Overdub;
      looper_switches::press_off_beat(h, in.rec_play_dub);
    }
    looper_switches::next_beat(h);
    const int64_t start = h.now - switches_harness::frames;
    looper_switches::beats(h, 3);
    looper_switches::press(h, in.rec_play_dub);
    looper_switches::next_beat(h);
    const int64_t end = h.now - switches_harness::frames;
    REQUIRE(h.length() == end - start);
    looper_switches::beats(h, 1);
    looper_switches::press_off_beat(h, in.play);
    looper_switches::next_beat(h);
    REQUIRE(h.passes() == 2);

    // Every frame is in the loop once: the recording up to its last frame,
    // not faded out, and the overdub from the next one, not faded in.
    const int64_t len = h.length();
    const float* rec = h.node.layer_data(0, 0);
    const float* dub = h.node.layer_data(1, 0);
    for(int64_t k : {int64_t(1000), len - 128, len - 2, len - 1})
    {
      INFO(k);
      CHECK(rec[k] == Approx(switches_harness::ramp_at(start + k)));
    }
    for(int64_t k : {int64_t(0), int64_t(1), int64_t(64), int64_t(127), int64_t(20000)})
    {
      INFO(k);
      CHECK(dub[k] == Approx(switches_harness::ramp_at(end + k)));
    }
    // What is heard goes on across the end of the loop as it was played.
    CHECK(h.node.layers.mix(0, len - 1) == Approx(switches_harness::ramp_at(end - 1)));
    CHECK(
        h.node.layers.mix(0, 0) - rec[0]
        == Approx(switches_harness::ramp_at(end)).margin(1e-6));
  }
}

TEST_CASE("Looper switches: the recording's end is faded while the overdub it went on "
          "into is not heard",
          "[fx][audio][looper][switches][layers]")
{
  enum how
  {
    by_undo,
    by_mute,
    by_erase
  };
  for(const how what : {by_undo, by_mute, by_erase})
  {
    INFO(int(what));
    switches_harness h;
    h.ramp = true;
    auto& in = h.node.inputs;
    in.after_record.value = Nodes::Looper::AfterRecord::Overdub;
    looper_switches::press_off_beat(h, in.rec_play_dub);
    looper_switches::next_beat(h);
    const int64_t start = h.now - switches_harness::frames;
    looper_switches::beats(h, 3);
    looper_switches::press(h, in.rec_play_dub);
    looper_switches::next_beat(h);
    looper_switches::beats(h, 1);
    looper_switches::press_off_beat(h, in.play);
    looper_switches::next_beat(h);
    REQUIRE(h.passes() == 2);

    const int64_t len = h.length();
    const float* rec = h.node.layer_data(0, 0);
    auto played = [&](int64_t k) { return switches_harness::ramp_at(start + k); };
    auto unfaded = [&] {
      CHECK(rec[len - 1] == Approx(played(len - 1)));
      CHECK(rec[len - 64] == Approx(played(len - 64)));
    };
    auto faded = [&] {
      CHECK(rec[len - 1] == Approx(played(len - 1) / 128.).margin(1e-4));
      CHECK(rec[len - 64] == Approx(played(len - 64) * 64. / 128.).margin(1e-4));
      CHECK(rec[len - 129] == Approx(played(len - 129)));
    };
    unfaded();

    switch(what)
    {
      case by_undo:
        looper_switches::press(h, in.undo);
        REQUIRE(h.passes() == 1);
        faded();
        looper_switches::press(h, in.redo);
        REQUIRE(h.passes() == 2);
        unfaded();
        // Undone, then replaced by another overdub: the seam is gone.
        looper_switches::press(h, in.undo);
        in.mode = LoopMode::Overdub;
        looper_switches::beats(h, 1);
        in.mode = LoopMode::Play;
        looper_switches::beats(h, 1);
        REQUIRE(h.passes() == 2);
        faded();
        break;
      case by_mute:
        looper_switches::press(h, in.mute_layer);
        looper_switches::next_beat(h);
        REQUIRE(h.muted(1));
        faded();
        looper_switches::press(h, in.mute_layer);
        looper_switches::next_beat(h);
        REQUIRE(!h.muted(1));
        unfaded();
        break;
      case by_erase:
        looper_switches::press(h, in.erase_layer);
        looper_switches::next_beat(h);
        REQUIRE(h.passes() == 1);
        faded();
        looper_switches::press(h, in.redo);
        faded();
        break;
    }
  }
}

TEST_CASE("Looper switches: the actions allocate nothing", "[fx][audio][looper][switches]")
{
  switches_harness h;
  int layer{};
  bool reverse{};
  float speed{};
  h.node.inputs.layer.update_controller = [&layer](int v) { layer = v; };
  h.node.inputs.reverse.update_controller = [&reverse](bool v) { reverse = v; };
  h.node.inputs.speed.update_controller = [&speed](float v) { speed = v; };
  int allocated = -1;
  {
    alloc_counter::scope counter;
    looper_switches::exercise(h);
    allocated = counter.allocations();
  }
  CHECK(allocated == 0);
  // The last recording, overdubbed.
  CHECK(h.passes() == 2);
  CHECK(h.shown_count > 5);
}

namespace
{
std::shared_ptr<Nodes::Looper::take_writer> temp_take_writer(const QTemporaryDir& dir)
{
  auto writer = std::make_shared<Nodes::Looper::take_writer>();
  writer->folder = dir.path();
  writer->prefix = "t";
  writer->extension = "wav";
  return writer;
}
}

TEST_CASE("Looper: a take asked for while overdubbing is the loop of that moment",
          "[fx][audio][looper][takes]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  // Long enough to be flattened over several ticks, while the overdub goes
  // on writing over what it wrote on its first round.
  constexpr int64_t len = 600000;
  looper_harness h{1, 4096};
  h.node.writer = temp_take_writer(dir);
  h.node.worker.request = [&](Nodes::Looper::take_job job) {
    Looper::worker_t::work(std::move(job))(h.node);
  };
  auto view = std::make_unique<Nodes::Looper::view_message>();
  h.node.send_message = [&](const Nodes::Looper::view_message& m) { *view = m; };
  h.node.inputs.feedback.value = 1.f;
  h.node.inputs.exported.value = Nodes::Looper::TakeFiles::Mix;

  auto run_to = [&](int64_t frames) {
    for(int64_t done = 0; done < frames; done += h.buffer_size)
      h.run(std::min<int64_t>(h.buffer_size, frames - done));
  };
  for(auto& v : h.in_buf[0])
    v = 0.25;
  h.node.inputs.mode = LoopMode::Record;
  run_to(len);
  for(auto& v : h.in_buf[0])
    v = 0.5;
  h.node.inputs.mode = LoopMode::Overdub;
  run_to(len);
  REQUIRE(view->take == 1);
  for(auto& v : h.in_buf[0])
    v = 1.;
  run_to(len / 2);

  h.node.process_message({.pass = -1, .layers = -1, .mute = false, .take = 1});
  h.run(h.buffer_size);
  // The loop as it is heard once the tick that took the request is over.
  std::vector<float> heard(len);
  for(int64_t k = 0; k < len; k++)
    heard[k] = h.node.layers.mix(0, k);
  for(int i = 0; i < 40 && view->answered < 1; i++)
    h.run(h.buffer_size);
  REQUIRE(view->answered == 1);
  REQUIRE(view->take == 2);
  CHECK(h.node.transport.actualMode == LoopMode::Overdub);
  // The overdub went on meanwhile.
  CHECK(h.node.layers.mix(0, (h.node.transport.playbackPos + len - 1000) % len)
        != Approx(heard[(h.node.transport.playbackPos + len - 1000) % len]));

  const auto take = read_wav(QString(dir.path() + "/t-take2.wav").toStdString());
  REQUIRE(take.channels > 0);
  REQUIRE(take.samples.size() == std::size_t(len) * std::size_t(take.channels));
  int differ = 0;
  for(int64_t k = 0; k < len; k += 97)
    differ += std::abs(take.samples[k * take.channels] - heard[k]) > 1e-4;
  CHECK(differ == 0);
  // Both rounds of the overdub are in it, where the playhead was.
  CHECK(heard[len / 4] == Approx(1.75));
  CHECK(heard[3 * len / 4] == Approx(0.75));
}

TEST_CASE("Looper: the score stopping while a pass is taken in writes a take of the loop "
          "as it is heard",
          "[fx][audio][looper][takes]")
{
  for(const bool recording : {false, true})
  {
    INFO(recording);
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    std::vector<Nodes::Looper::take_job> jobs;
    {
      auto h = std::make_unique<looper_harness>(1, 64);
      h->node.writer = temp_take_writer(dir);
      h->node.worker.request = [&](Nodes::Looper::take_job job) {
        jobs.push_back(std::move(job));
      };
      for(auto& v : h->in_buf[0])
        v = 0.25;
      h->node.inputs.mode = LoopMode::Record;
      for(int i = 0; i < 8; i++)
        h->run(64);
      if(!recording)
      {
        for(auto& v : h->in_buf[0])
          v = 0.5;
        h->node.inputs.mode = LoopMode::Overdub;
        for(int i = 0; i < 6; i++)
          h->run(64);
        // The take of the recording: written by the time the score stops.
        REQUIRE(jobs.size() == 1);
        Looper::worker_t::work(std::move(jobs[0]))(h->node);
        jobs.clear();
      }
      REQUIRE(jobs.empty());
      h->node.stop();
    }
    // Flattened and written once the looper is gone: the processing thread
    // only handed the passes over.
    REQUIRE(jobs.size() == 1);
    CHECK(jobs[0].alone);
    CHECK(jobs[0].audio->flatten);
    REQUIRE(jobs[0].run());
    const auto name = recording ? "/t-take1.wav" : "/t-take2.wav";
    const auto take = read_wav(QString(dir.path() + name).toStdString());
    REQUIRE(take.channels > 0);
    REQUIRE(take.samples.size() == 512 * std::size_t(take.channels));
    CHECK(take.samples[200 * take.channels] == Approx(recording ? 0.25 : 0.75));
    CHECK(take.samples[450 * take.channels] == Approx(0.25));
  }
}

TEST_CASE("Looper: a take asked for while overdubbing allocates nothing",
          "[fx][audio][looper][takes]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  looper_harness h{1, 64};
  h.node.writer = temp_take_writer(dir);
  std::vector<Nodes::Looper::take_job> jobs;
  jobs.reserve(16);
  h.node.worker.request = [&](Nodes::Looper::take_job job) {
    jobs.push_back(std::move(job));
  };
  record_and_overdub(h, 512, 0.25, 0.5);
  h.node.inputs.mode = LoopMode::Overdub;
  h.run(64);
  // The takes the passes closed with are written.
  for(int i = 0; i < 4 && !jobs.empty(); i++)
  {
    auto done = std::move(jobs);
    jobs.clear();
    jobs.reserve(16);
    for(auto& j : done)
      Looper::worker_t::work(std::move(j))(h.node);
    h.run(64);
  }
  jobs.clear();
  int allocated = -1;
  {
    alloc_counter::scope counter;
    h.node.process_message({.pass = -1, .layers = -1, .mute = false, .take = 1});
    for(int i = 0; i < 4; i++)
      h.run(64);
    allocated = counter.allocations();
  }
  CHECK(allocated == 0);
  CHECK(jobs.size() == 1);
}

TEST_CASE("Looper switches: Quantif on Loop waits for the loop's start", "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::quantif_loop(h);
}

TEST_CASE("Looper switches: Quantif on Loop before there is a loop is Bars bars", "[fx][audio][looper][switches]")
{
  switches_harness h;
  looper_switches::quantif_loop_empty(h);
}

namespace
{
//! The frame a recording asked for at `press_at` starts on, with Quantif on
//! `grid`: ticks of a thousand frames, so the frame can be read from the
//! length recorded so far.
int64_t recording_start(halp::grid_value grid, int64_t press_at, int upper = 4)
{
  musical_harness h{1000};
  h.upper = upper;
  h.node.inputs.quantif.value = grid;
  h.node.inputs.postaction_bars.value = 0;
  h.node.inputs.late.value = 0;
  h.node.inputs.passthrough = Passthrough::None;
  h.node.inputs.mode = LoopMode::Stop;
  while(h.now < press_at)
    h.run(h.mtick(h.buffer_size));
  h.node.inputs.mode = LoopMode::Record;
  while(h.node.transport.actualMode != LoopMode::Record && h.now < 20 * h.bar_samples())
    h.run(h.mtick(h.buffer_size));
  return h.now - h.loop().size();
}
}

TEST_CASE("Looper: Quantif in bars, note values and triplets", "[fx][audio][looper][musical]")
{
  // 120 bpm at 48 kHz: a quarter is 24000 frames, a bar of 4/4 96000.
  SECTION("3 bars: on the third bar, not the next one")
  {
    CHECK(recording_start(halp::grid_value::bars(3), 100000) == 288000);
  }
  SECTION("1/1 in 3/4 is four quarters, not a bar")
  {
    CHECK(recording_start(halp::grid_value::note(1.), 10000, 3) == 96000);
  }
  SECTION("1/4 in 3/4 is a quarter, not a quarter of the bar")
  {
    CHECK(recording_start(halp::grid_value::note(0.25), 30000, 3) == 48000);
  }
  SECTION("1 bar in 3/4 is three quarters")
  {
    CHECK(recording_start(halp::grid_value::bars(1), 10000, 3) == 72000);
  }
  SECTION("1/8T: three to the quarter")
  {
    CHECK(recording_start(halp::grid_value::note(1. / 12.), 10000) == 16000);
  }
  SECTION("1/4T: three to the half note")
  {
    CHECK(recording_start(halp::grid_value::note(1. / 6.), 20000) == 32000);
  }
  SECTION("A time in seconds, from the start of playback before there is a loop")
  {
    CHECK(recording_start(halp::grid_value::seconds(0.5), 30000) == 48000);
  }
}

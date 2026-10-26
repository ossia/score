// The looper presets of the default package: each one loads onto its looper,
// sets every setting, leaves the transport alone, and does what its name says
// when the looper is played with it.

#include "MidiPresetHarness.hpp"

#include <Fx/Looper/AudioLooper.hpp>
#include <Fx/Looper/MidiLooper.hpp>

#include <ossia/dataflow/execution_state.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <set>
#include <string>
#include <vector>

using Catch::Approx;

namespace
{
using Audio = Nodes::AudioLooper::Node;
using Midi = Nodes::MidiLooper::Node;
using LoopMode = Nodes::Looper::LoopMode;
using AfterRecord = Nodes::Looper::AfterRecord;
using AfterOverdub = Nodes::Looper::AfterOverdub;
using AtMaxLayers = Nodes::Looper::AtMaxLayers;
using Passes = Nodes::Looper::Passes;
using PastEnd = Nodes::Looper::PastEnd;
using StartFrom = Nodes::Looper::StartFrom;
using StopAt = Nodes::Looper::StopAt;
using TakeFiles = Nodes::Looper::TakeFiles;

const std::vector<midi_presets::preset>& audio_presets()
{
  static const auto p
      = midi_presets::load("Looper (audio)", "a0ad4227-ac3d-448b-a19b-19581ed4e2c6");
  return p;
}
const std::vector<midi_presets::preset>& midi_presets_list()
{
  static const auto p
      = midi_presets::load("Looper (MIDI)", "0c0b7d3e-5a52-4b8f-9d1f-6e3a2b8c4f71");
  return p;
}

template <typename Node>
const std::vector<midi_presets::preset>& presets_of()
{
  if constexpr(std::is_same_v<Node, Audio>)
    return audio_presets();
  else
    return midi_presets_list();
}

template <typename Node>
void apply(Node& n, const char* name)
{
  for(auto& p : presets_of<Node>())
  {
    if(p.name == name)
    {
      const auto errors = midi_presets::apply(n.inputs, p);
      for(auto& e : errors)
        UNSCOPED_INFO(e.toStdString());
      CHECK(errors.empty());
      return;
    }
  }
  FAIL("missing preset " << name);
}

//! The index a preset gives the input `field` of `inputs` by.
template <typename Inputs, typename Field>
int index_of(Inputs& inputs, const Field& field)
{
  int i = 0, found = -1;
  avnd::pfr::for_each_field(inputs, [&](auto& f) {
    if(static_cast<const void*>(&f) == static_cast<const void*>(&field))
      found = i;
    i++;
  });
  return found;
}

std::set<int> indices(const midi_presets::preset& p)
{
  std::set<int> res;
  for(const auto& e : p.values)
    res.insert(e.toArray()[0].toInt());
  return res;
}

// 4/4 at 120 bpm, 48 kHz: a bar is 96000 frames, twenty ticks.
constexpr int64_t rig_frames = 4800;
constexpr int64_t rig_bar = 96000;

ossia::token_request musical_tick(int64_t now, int64_t frames)
{
  ossia::token_request tk;
  tk.prev_date = ossia::time_value{now};
  tk.date = ossia::time_value{now + frames};
  tk.start_sample = 0;
  tk.length_sample = frames;
  tk.tempo = 120.;
  tk.signature = {4, 4};
  const double qps = 2. / 48000.;
  tk.musical_start_position = now * qps;
  tk.musical_end_position = (now + frames) * qps;
  tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
  tk.musical_end_last_bar = std::floor(tk.musical_end_position / 4.) * 4.;
  tk.musical_start_last_signature = 0.;
  return tk;
}

void setup_state(ossia::execution_state& st)
{
  st.sampleRate = 48000;
  st.bufferSize = rig_frames;
  st.modelToSamplesRatio = 1.;
  st.samplesToModelRatio = 1.;
}

//! The audio looper with a preset, as a start prepares it, its input a
//! constant 0.5 on both channels.
struct audio_rig
{
  static constexpr int64_t frames = rig_frames;
  static constexpr int64_t bar = rig_bar;
  Audio node;
  ossia::execution_state st;
  std::array<std::vector<double>, 2> in_buf, out_buf;
  std::array<double*, 2> in_ptr{}, out_ptr{};
  int64_t now{};

  explicit audio_rig(const char* preset)
  {
    setup_state(st);
    node.ossia_state = {&st};
    apply(node, preset);
    node.prepare(halp::setup{
        .input_channels = 2, .output_channels = 2, .frames = int(frames), .rate = 48000.});
    for(int c = 0; c < 2; c++)
    {
      in_buf[c].assign(frames, 0.5);
      out_buf[c].assign(frames, 0.);
    }
    node.inputs.mode = LoopMode::Stop;
    for(int i = 0; i < 5; i++)
      tick_once();
  }

  void tick_once()
  {
    const auto tk = musical_tick(now, frames);
    now += frames;
    // The engine's clock is at the end of the buffer while it runs.
    st.samples_since_start = now;
    for(int c = 0; c < 2; c++)
    {
      in_ptr[c] = in_buf[c].data();
      out_ptr[c] = out_buf[c].data();
    }
    node.inputs.audio.samples = in_ptr.data();
    node.inputs.audio.channels = 2;
    node.outputs.audio.samples = out_ptr.data();
    node.outputs.audio.channels = 2;
    node(tk);
  }
  int passes() const { return node.layers.count; }
  bool muted(int k) const { return node.layers.layers[k].muted; }
  int64_t length() const { return node.layers.length; }
  int64_t position() const
  {
    return node.transport.playbackPos % std::max<int64_t>(1, node.layers.length);
  }
};

//! The MIDI looper with a preset; a short note is played on each tick on
//! `channel` while `playing`, its pitch going round an octave from 60.
struct midi_rig
{
  static constexpr int64_t frames = rig_frames;
  static constexpr int64_t bar = rig_bar;
  Midi node;
  ossia::execution_state st;
  int64_t now{};
  bool playing{true};
  int channel{1};
  int pitch{};
  std::vector<int> pitches_out;

  explicit midi_rig(const char* preset)
  {
    setup_state(st);
    node.ossia_state = {&st};
    node.inputs.midi.midi_messages.reserve(64);
    node.outputs.midi.midi_messages.reserve(1024);
    apply(node, preset);
    node.prepare(halp::setup{.frames = int(frames), .rate = 48000.});
    node.inputs.mode = LoopMode::Stop;
    for(int i = 0; i < 5; i++)
      tick_once();
  }

  void tick_once()
  {
    auto& in = node.inputs.midi.midi_messages;
    in.clear();
    if(playing)
    {
      auto on = libremidi::channel_events::note_on(channel, 60 + pitch, 100);
      on.timestamp = 100;
      auto off = libremidi::channel_events::note_off(channel, 60 + pitch, 0);
      off.timestamp = 1000;
      in.push_back(on);
      in.push_back(off);
      pitch = (pitch + 1) % 12;
    }
    node.outputs.midi.midi_messages.clear();
    const auto tk = musical_tick(now, frames);
    st.samples_since_start = now + frames;
    node(tk);
    for(auto& m : node.outputs.midi.midi_messages)
      if(mtk::classify(m) == mtk::midi_kind::note_on)
        pitches_out.push_back(m.bytes[1]);
    now += frames;
  }
  int passes() const { return node.loop.count; }
  bool muted(int k) const { return node.loop.muted[k]; }
  int64_t length() const { return node.loop.length; }
  int64_t position() const
  {
    return node.transport.playbackPos % std::max<int64_t>(1, node.loop.length);
  }
};

template <typename R>
LoopMode mode(const R& r)
{
  return r.node.transport.actualMode;
}

//! Frames of the preset's grid; zero without one.
template <typename R>
int64_t grid(const R& r)
{
  return int64_t(std::llround(r.node.inputs.quantif.value * double(R::bar)));
}

template <typename R>
void ticks(R& r, int64_t n)
{
  for(int64_t i = 0; i < n; i++)
    r.tick_once();
}

template <typename R>
void bars(R& r, int n)
{
  ticks(r, n * (R::bar / R::frames));
}

template <typename R>
void press(R& r, auto& button)
{
  button.value.emplace();
  r.tick_once();
  button.value.reset();
}

//! Runs to the middle of a grid step: far from the late stop of its point.
template <typename R>
void to_middle(R& r)
{
  if(const int64_t g = grid(r); g > 0)
    while(r.now % g != g / 2)
      r.tick_once();
}

//! Runs up to the next grid point, and the tick that starts on it.
template <typename R>
void to_point(R& r)
{
  if(const int64_t g = grid(r); g > 0)
    while(r.now % g != 0)
      r.tick_once();
  r.tick_once();
}

//! Pressed between two grid points, taken up on the next one.
template <typename R>
void act(R& r, auto& button)
{
  to_middle(r);
  press(r, button);
  to_point(r);
}

//! Records `n` bars with Rec / Play / Dub, ending them by it.
template <typename R>
void record_bars(R& r, int n)
{
  auto& in = r.node.inputs;
  act(r, in.rec_play_dub);
  REQUIRE(mode(r) == LoopMode::Record);
  bars(r, n - 1);
  act(r, in.rec_play_dub);
  REQUIRE(r.length() == n * R::bar);
}

// What the names say, for both loopers.

template <typename R>
void song_form()
{
  R r{"Song form"};
  auto& in = r.node.inputs;
  CHECK(grid(r) == R::bar);
  record_bars(r, 2);
  CHECK(mode(r) == LoopMode::Play);
  act(r, in.rec_play_dub);
  CHECK(mode(r) == LoopMode::Overdub);
  CHECK(r.passes() == 2);
  act(r, in.rec_play_dub);
  CHECK(mode(r) == LoopMode::Play);
  CHECK(r.passes() == 2);
  // A stop asked for in the first bar of the loop lets it play to its end.
  CHECK(r.position() == R::frames);
  act(r, in.play_stop);
  CHECK(mode(r) == LoopMode::Play);
  bars(r, 1);
  CHECK(mode(r) == LoopMode::Stop);
}

template <typename R>
void one_footswitch_song_form()
{
  // Ended two ticks, 200 ms, after the bar line: on it, and played at once.
  const auto late_press = [](R& r) {
    auto& in = r.node.inputs;
    act(r, in.rec_play_dub);
    REQUIRE(mode(r) == LoopMode::Record);
    bars(r, 1);
    r.tick_once();
    press(r, in.rec_play_dub);
  };
  {
    R r{"One footswitch, song form"};
    auto& in = r.node.inputs;
    late_press(r);
    CHECK(mode(r) == LoopMode::Play);
    CHECK(r.length() == R::bar);
    act(r, in.rec_play_dub);
    CHECK(mode(r) == LoopMode::Overdub);
    act(r, in.rec_play_dub);
    CHECK(mode(r) == LoopMode::Play);
    CHECK(r.passes() == 2);
  }
  {
    // Without the footswitch's late stop, the press waits for the next bar.
    R r{"Song form"};
    late_press(r);
    CHECK(mode(r) == LoopMode::Record);
  }
}

//! Every tick from now until `n` bars on is taking in: no gap.
template <typename R>
bool takes_in_for(R& r, int n)
{
  bool always = true;
  for(int64_t i = 0; i < n * (R::bar / R::frames); i++)
  {
    r.tick_once();
    always = always && r.node.transport.is_taking()
             && (mode(r) == LoopMode::Record || mode(r) == LoopMode::Overdub);
  }
  return always;
}

template <typename R>
void one_footswitch_layering()
{
  R r{"One footswitch, layering"};
  auto& in = r.node.inputs;
  act(r, in.rec_play_dub);
  REQUIRE(mode(r) == LoopMode::Record);
  to_middle(r);
  press(r, in.rec_play_dub);
  CHECK(takes_in_for(r, 1));
  // The recording went straight on into an overdub, and each press after it
  // goes on in a new layer.
  CHECK(mode(r) == LoopMode::Overdub);
  CHECK(r.length() == R::bar);
  CHECK(r.passes() == 2);
  for(int i = 0; i < 2; i++)
  {
    act(r, in.rec_play_dub);
    CHECK(mode(r) == LoopMode::Overdub);
    CHECK(r.passes() == 3 + i);
  }
  act(r, in.play_stop);
  CHECK(mode(r) == LoopMode::Stop);
}

template <typename R>
void four_bars_then(const char* name, LoopMode after)
{
  R r{name};
  auto& in = r.node.inputs;
  CHECK(in.postaction_bars.value == 4);
  act(r, in.record);
  REQUIRE(mode(r) == LoopMode::Record);
  bars(r, 4);
  CHECK(mode(r) == after);
  CHECK(r.length() == 4 * R::bar);
  CHECK(r.passes() == (after == LoopMode::Overdub ? 2 : 1));
}

template <typename R>
void free_length(const char* name)
{
  R r{name};
  auto& in = r.node.inputs;
  CHECK(grid(r) == 0);
  CHECK(in.postaction_bars.value == 0);
  CHECK(in.late.value == 0);
  // Every change at once, on the tick it is asked on.
  press(r, in.record);
  CHECK(mode(r) == LoopMode::Record);
  ticks(r, 6);
  press(r, in.play);
  CHECK(mode(r) == LoopMode::Play);
  CHECK(r.length() == 7 * R::frames);
}

template <typename R>
void comping_per_press()
{
  R r{"Comping, a layer per press"};
  auto& in = r.node.inputs;
  const int max = in.max_layers.value;
  CHECK(max == 16);
  record_bars(r, 1);
  act(r, in.rec_play_dub);
  CHECK(mode(r) == LoopMode::Overdub);
  CHECK(r.passes() == 2);
  // A new layer at each press, the playhead going on.
  for(int i = 3; i <= max; i++)
  {
    const int64_t before = r.position();
    act(r, in.rec_play_dub);
    CHECK(mode(r) == LoopMode::Overdub);
    CHECK(r.passes() == i);
    CHECK(r.position() == before);
  }
  // At max layers, a take takes the place of the one muted.
  in.layer.value = 3;
  act(r, in.mute_layer);
  REQUIRE(r.muted(2));
  act(r, in.rec_play_dub);
  act(r, in.rec_play_dub);
  CHECK(r.passes() == max);
  int muted = 0;
  for(int k = 0; k < r.passes(); k++)
    muted += r.muted(k);
  CHECK(muted == 0);
}

template <typename R>
void comping_per_round()
{
  R r{"Comping, a layer per round"};
  auto& in = r.node.inputs;
  CHECK(in.exported.value == TakeFiles::Both);
  CHECK(in.at_max_layers.value == AtMaxLayers::ReplaceMuted);
  record_bars(r, 1);
  act(r, in.rec_play_dub);
  REQUIRE(r.passes() == 2);
  // One overdub, a layer each time round the loop.
  bars(r, 3);
  CHECK(mode(r) == LoopMode::Overdub);
  CHECK(r.passes() == 5);
}

template <typename R>
void half_speed()
{
  R r{"Half speed"};
  record_bars(r, 2);
  REQUIRE(mode(r) == LoopMode::Play);
  const int64_t before = r.position();
  bars(r, 1);
  CHECK((r.position() - before + r.length()) % r.length() == R::bar / 2);
}

template <typename R>
void grows_as_you_overdub()
{
  R r{"Grows as you overdub"};
  auto& in = r.node.inputs;
  act(r, in.record);
  bars(r, 1);
  REQUIRE(mode(r) == LoopMode::Play);
  REQUIRE(r.length() == R::bar);
  // Overdubbed over three bars, the bar long loop is three bars long.
  act(r, in.overdub);
  bars(r, 2);
  act(r, in.play);
  CHECK(mode(r) == LoopMode::Play);
  CHECK(r.length() == 3 * R::bar);
}

//! Stopped for a bar, a two bar loop comes back a bar on with In phase, where
//! it stopped otherwise.
template <typename R>
int64_t played_again_at(const char* name)
{
  R r{name};
  auto& in = r.node.inputs;
  record_bars(r, 2);
  act(r, in.stop);
  REQUIRE(mode(r) == LoopMode::Stop);
  REQUIRE(r.position() == R::bar);
  act(r, in.play);
  REQUIRE(mode(r) == LoopMode::Play);
  return r.position() - R::frames;
}

template <typename R>
void in_phase()
{
  CHECK(played_again_at<R>("In phase with the timeline") == 0);
  CHECK(played_again_at<R>("One footswitch, song form") == R::bar);
}

template <typename R>
void takes_for_editing()
{
  R r{"Takes for editing"};
  CHECK(r.node.inputs.exported.value == TakeFiles::Both);
}
}

TEST_CASE("Looper presets: every one loads, sets every setting, and none starts the "
          "looper",
          "[fx][looper][presets]")
{
  REQUIRE(audio_presets().size() == 16);
  REQUIRE(midi_presets_list().size() == 19);
  {
    Audio n;
    const auto all = indices(audio_presets().front());
    for(const int f : {index_of(n.inputs, n.inputs.after_record),
                   index_of(n.inputs, n.inputs.start_from),
                   index_of(n.inputs, n.inputs.after_overdub),
                   index_of(n.inputs, n.inputs.stop_at), index_of(n.inputs, n.inputs.passes),
                   index_of(n.inputs, n.inputs.at_max_layers)})
      CHECK(all.count(f));
  }
  for(auto& p : audio_presets())
  {
    Audio n;
    n.inputs.mode.value = LoopMode::Stop;
    INFO(p.name.toStdString());
    CHECK(midi_presets::apply(n.inputs, p).empty());
    // Loading one after another leaves nothing of the one before.
    CHECK(indices(p) == indices(audio_presets().front()));
    CHECK(n.inputs.mode.value == LoopMode::Stop);
    CHECK(!n.inputs.undo.value);
    CHECK(!n.inputs.capture.value);
    CHECK(
        n.inputs.start_from.value
        == (p.name == "In phase with the timeline" ? StartFrom::InPhase
                                                     : StartFrom::WhereItWas));
  }
  for(auto& p : midi_presets_list())
  {
    Midi n;
    n.inputs.mode.value = LoopMode::Stop;
    INFO(p.name.toStdString());
    CHECK(midi_presets::apply(n.inputs, p).empty());
    CHECK(indices(p) == indices(midi_presets_list().front()));
    CHECK(indices(p).count(index_of(n.inputs, n.inputs.at_max_layers)));
    CHECK(n.inputs.mode.value == LoopMode::Stop);
    CHECK(!n.inputs.capture.value);
    CHECK(
        n.inputs.start_from.value
        == (p.name == "In phase with the timeline" ? StartFrom::InPhase
                                                     : StartFrom::WhereItWas));
  }
}

TEST_CASE("Looper presets: what the names say, the settings", "[fx][looper][presets]")
{
  {
    Audio n;
    apply(n, "Free loop");
    CHECK(n.inputs.postaction_bars.value == 0);
    CHECK(n.inputs.quantif.value == Approx(0.f));
  }
  {
    Audio n;
    apply(n, "Second loop of group A");
    CHECK(n.inputs.sync.value == Nodes::Looper::SyncGroup::A);
  }
  {
    Midi n;
    apply(n, "Second loop of group A");
    CHECK(n.inputs.sync.value == Nodes::Looper::SyncGroup::A);
  }
  {
    Midi n;
    apply(n, "Replace as you play");
    CHECK(n.inputs.overdub_mode.value == Nodes::Looper::OverdubMode::Replace);
    CHECK(n.inputs.after_record.value == AfterRecord::Overdub);
  }
  {
    Midi n;
    apply(n, "Overdub drums");
    CHECK(n.inputs.held.value == Nodes::Looper::HeldNotes::Truncate);
    CHECK(n.inputs.postaction_bars.value == 2);
    CHECK(n.inputs.after_record.value == AfterRecord::Overdub);
  }
  {
    Midi n;
    apply(n, "Sixteenths, 70 percent");
    CHECK(n.inputs.grid.value == Approx(1.f / 16.f));
    CHECK(n.inputs.strength.value == Approx(0.7f));
  }
  {
    Audio n;
    apply(n, "Comping, a layer per press");
    CHECK(n.inputs.after_overdub.value == AfterOverdub::NewLayer);
    CHECK(n.inputs.at_max_layers.value == AtMaxLayers::ReplaceMuted);
    // Twice the layers of the default, for half the length: the same memory.
    CHECK(n.inputs.max_layers.value * n.inputs.max_seconds.value == 8 * 60);
  }
  {
    Audio n;
    apply(n, "Comping, a layer per round");
    CHECK(n.inputs.passes.value == Passes::PerCycle);
    CHECK(n.inputs.max_layers.value * n.inputs.max_seconds.value == 8 * 60);
  }
  {
    Audio n;
    apply(n, "Song form");
    CHECK(n.inputs.stop_at.value == StopAt::LoopEnd);
    CHECK(n.inputs.after_overdub.value == AfterOverdub::Play);
  }
}

TEST_CASE("Looper presets: song form", "[fx][looper][presets]")
{
  SECTION("audio") { song_form<audio_rig>(); }
  SECTION("MIDI") { song_form<midi_rig>(); }
}

TEST_CASE("Looper presets: one footswitch, song form", "[fx][looper][presets]")
{
  SECTION("audio") { one_footswitch_song_form<audio_rig>(); }
  SECTION("MIDI") { one_footswitch_song_form<midi_rig>(); }
}

TEST_CASE("Looper presets: one footswitch, layering", "[fx][looper][presets]")
{
  SECTION("audio") { one_footswitch_layering<audio_rig>(); }
  SECTION("MIDI") { one_footswitch_layering<midi_rig>(); }
}

TEST_CASE("Looper presets: four bars, then play or overdub", "[fx][looper][presets]")
{
  four_bars_then<audio_rig>("Four bars, then play", LoopMode::Play);
  four_bars_then<audio_rig>("Four bars, then overdub", LoopMode::Overdub);
  four_bars_then<midi_rig>("Four bar phrase", LoopMode::Play);
  four_bars_then<midi_rig>("Four bars, then overdub", LoopMode::Overdub);
}

TEST_CASE("Looper presets: free length", "[fx][looper][presets]")
{
  free_length<audio_rig>("Free loop");
  free_length<midi_rig>("Free phrase");
}

TEST_CASE("Looper presets: comping", "[fx][looper][presets]")
{
  SECTION("a layer per press, audio") { comping_per_press<audio_rig>(); }
  SECTION("a layer per press, MIDI") { comping_per_press<midi_rig>(); }
  SECTION("a layer per round, audio") { comping_per_round<audio_rig>(); }
  SECTION("a layer per round, MIDI") { comping_per_round<midi_rig>(); }
}

TEST_CASE("Looper presets: tape tricks", "[fx][looper][presets]")
{
  SECTION("half speed, audio") { half_speed<audio_rig>(); }
  SECTION("half speed, MIDI") { half_speed<midi_rig>(); }
  SECTION("grows as you overdub, audio") { grows_as_you_overdub<audio_rig>(); }
  SECTION("grows as you overdub, MIDI") { grows_as_you_overdub<midi_rig>(); }
  SECTION("played backwards")
  {
    audio_rig r{"Played backwards"};
    record_bars(r, 1);
    REQUIRE(mode(r) == LoopMode::Play);
    const int64_t before = r.position();
    r.tick_once();
    CHECK(r.position() == (before - audio_rig::frames + r.length()) % r.length());
  }
}

TEST_CASE("Looper presets: in phase with the timeline", "[fx][looper][presets]")
{
  SECTION("audio") { in_phase<audio_rig>(); }
  SECTION("MIDI") { in_phase<midi_rig>(); }
}

TEST_CASE("Looper presets: takes for editing", "[fx][looper][presets]")
{
  takes_for_editing<audio_rig>();
  takes_for_editing<midi_rig>();
}

TEST_CASE("Looper presets: ambient decay goes straight into a fading overdub",
          "[fx][looper][presets]")
{
  audio_rig r{"Ambient decay"};
  auto& in = r.node.inputs;
  CHECK(in.feedback.value < 1.f);
  act(r, in.rec_play_dub);
  REQUIRE(mode(r) == LoopMode::Record);
  // Four bars of recording, and the overdub from the very frame it ends on.
  CHECK(takes_in_for(r, 5));
  CHECK(mode(r) == LoopMode::Overdub);
  CHECK(r.length() == 4 * audio_rig::bar);
  CHECK(r.passes() == 2);
  // Round the loop, the recording under the overdub fades.
  bars(r, 4);
  CHECK(r.node.layers.layers[0].gain == Approx(in.feedback.value).margin(1e-4));
}

TEST_CASE("Looper presets: capture what I played", "[fx][looper][presets]")
{
  SECTION("audio")
  {
    audio_rig r{"Capture what I played"};
    CHECK(r.node.inputs.keep.value > 0);
    bars(r, 5);
    to_middle(r);
    press(r, r.node.inputs.capture);
    CHECK(mode(r) == LoopMode::Play);
    CHECK(r.length() == r.node.inputs.capture_bars.value * audio_rig::bar);
  }
  SECTION("MIDI")
  {
    midi_rig r{"Capture what I played"};
    bars(r, 5);
    to_middle(r);
    press(r, r.node.inputs.capture);
    CHECK(mode(r) == LoopMode::Play);
    CHECK(r.length() == r.node.inputs.capture_bars.value * midi_rig::bar);
    CHECK(r.node.loop.note_count > 0);
  }
}

TEST_CASE("Looper presets: MIDI notes", "[fx][looper][presets]")
{
  SECTION("drum pattern on channel 10: only channel 10 is recorded")
  {
    for(const int ch : {1, 10})
    {
      midi_rig r{"Drum pattern on channel 10"};
      CHECK(r.node.inputs.overdub_mode.value == Nodes::Looper::OverdubMode::Replace);
      CHECK(r.node.inputs.held.value == Nodes::Looper::HeldNotes::Truncate);
      CHECK(r.node.inputs.strength.value == Approx(1.f));
      r.channel = ch;
      act(r, r.node.inputs.record);
      bars(r, 2);
      CHECK(mode(r) == LoopMode::Overdub);
      CHECK((r.node.loop.note_count > 0) == (ch == 10));
    }
  }
  SECTION("octave up: the loop plays an octave above what was recorded")
  {
    midi_rig r{"Octave up"};
    record_bars(r, 1);
    r.playing = false;
    r.pitches_out.clear();
    bars(r, 1);
    REQUIRE(!r.pitches_out.empty());
    for(int p : r.pitches_out)
      CHECK((p >= 72 && p < 84));
  }
}

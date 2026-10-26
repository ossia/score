// The looper presets of the default package: each one loads onto its looper,
// sets what its name says, and leaves the transport alone.

#include "MidiPresetHarness.hpp"

#include <Fx/Looper/AudioLooper.hpp>
#include <Fx/Looper/MidiLooper.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

namespace
{
using Audio = Nodes::AudioLooper::Node;
using Midi = Nodes::MidiLooper::Node;
using LoopMode = Nodes::Looper::LoopMode;
using AfterRecord = Nodes::Looper::AfterRecord;

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
void apply(Node& n, const std::vector<midi_presets::preset>& all, const char* name)
{
  for(auto& p : all)
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
}

TEST_CASE("Looper presets: every one loads, and none starts the looper",
          "[fx][looper][presets]")
{
  REQUIRE(audio_presets().size() == 6);
  REQUIRE(midi_presets_list().size() == 7);
  for(auto& p : audio_presets())
  {
    Audio n;
    n.inputs.mode.value = LoopMode::Stop;
    INFO(p.name.toStdString());
    CHECK(midi_presets::apply(n.inputs, p).empty());
    CHECK(n.inputs.mode.value == LoopMode::Stop);
    CHECK(!n.inputs.undo.value);
    CHECK(!n.inputs.capture.value);
  }
  for(auto& p : midi_presets_list())
  {
    Midi n;
    n.inputs.mode.value = LoopMode::Stop;
    INFO(p.name.toStdString());
    CHECK(midi_presets::apply(n.inputs, p).empty());
    CHECK(n.inputs.mode.value == LoopMode::Stop);
    CHECK(!n.inputs.capture.value);
  }
}

TEST_CASE("Looper presets: what the names say", "[fx][looper][presets]")
{
  {
    Audio n;
    apply(n, audio_presets(), "Four bars, then overdub");
    CHECK(n.inputs.postaction_bars.value == 4);
    CHECK(n.inputs.after_record.value == AfterRecord::Overdub);
    CHECK(n.inputs.quantif.value == Approx(1.f));
  }
  {
    Audio n;
    apply(n, audio_presets(), "Free loop");
    CHECK(n.inputs.postaction_bars.value == 0);
    CHECK(n.inputs.quantif.value == Approx(0.f));
  }
  {
    Audio n;
    apply(n, audio_presets(), "Ambient decay");
    CHECK(n.inputs.feedback.value == Approx(0.7f));
  }
  {
    Audio n;
    apply(n, audio_presets(), "Second loop of group A");
    CHECK(n.inputs.sync.value == Nodes::Looper::SyncGroup::A);
  }
  {
    Audio n;
    apply(n, audio_presets(), "Capture what I played");
    CHECK(n.inputs.keep.value == 60);
    CHECK(n.inputs.capture_bars.value == 4);
  }
  {
    Midi n;
    apply(n, midi_presets_list(), "Replace as you play");
    CHECK(n.inputs.overdub_mode.value == Nodes::Looper::OverdubMode::Replace);
  }
  {
    Midi n;
    apply(n, midi_presets_list(), "Overdub drums");
    CHECK(n.inputs.held.value == Nodes::Looper::HeldNotes::Truncate);
    CHECK(n.inputs.postaction_bars.value == 2);
  }
  {
    Midi n;
    apply(n, midi_presets_list(), "Sixteenths, 70 percent");
    CHECK(n.inputs.grid.value == Approx(1.f / 16.f));
    CHECK(n.inputs.strength.value == Approx(0.7f));
  }
}

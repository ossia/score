#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
namespace Nodes::Debug
{
struct Node
{
  halp_meta(name, "Test FX")
  halp_meta(c_name, "TestFX")
  halp_meta(category, "Debug")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(description, "Shows all the available widgets")
  halp_meta(uuid, "887507d3-8a56-4634-9ee3-a25d38050335")

  struct
  {

  } inputs;
  struct
  {

  } outputs;
  static const constexpr value_in value_ins[]{"in1", "in2"};
  static const constexpr value_out value_outs[]{"out1", "out2"};
  static const constexpr audio_in audio_ins[]{"ain"};
  static const constexpr audio_out audio_outs[]{"aout"};
  static const constexpr midi_in midi_ins[]{"min"};
  static const constexpr midi_out midi_outs[]{"mout"};

  static const constexpr auto controls = tuplet::make_tuple(Control::FloatSlider{"FloatSlider", -10, 20, 5}, Control::LogFloatSlider{"LogFloatSlider", -10, 20, 5}, Control::FloatKnob{"FloatKnob", -10, 20, 5}, Control::LogFloatKnob{"LogFloatKnob", -10, 20, 5}, Control::IntSlider{"IntSlider", -10, 20, 5}, Control::IntSpinBox{"IntSpinBox", -10, 20, 5}, Control::Toggle{"Toggle", true}, Control::ChooserToggle{"ChooserToggle", {"false", "true"}, true}, Control::LineEdit{"LineEdit", "henlo"}, Control::Button{"Bango"}, Control::Widgets::QuantificationChooser(), Control::Widgets::MusicalDurationChooser(), Control::Widgets::DurationChooser(), Control::Widgets::FreqSlider(), Control::Widgets::LFOFreqSlider(), Control::Widgets::FreqKnob(), Control::Widgets::LFOFreqKnob(), Control::Widgets::WaveformChooser());

  using control_policy = ossia::safe_nodes::default_tick_controls;
  template <typename... Args>
  static void run(Args&&...)
  {
  }
};
}

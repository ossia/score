#pragma once
#include <Engine/Node/PdNode.hpp>
namespace Nodes::Debug
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Test FX";
    static const constexpr auto objectKey = "TestFX";
    static const constexpr auto category = "Debug";
    static const constexpr auto author = "ossia score";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto kind = Process::ProcessCategory::Other;
    static const constexpr auto description
        = "Shows all the available widgets";
    static const uuid_constexpr auto uuid = make_uuid("887507d3-8a56-4634-9ee3-a25d38050335");

    static const constexpr value_in value_ins[]{"in1", "in2"};
    static const constexpr value_out value_outs[]{"out1", "out2"};
    static const constexpr audio_in audio_ins[]{"ain"};
    static const constexpr audio_out audio_outs[]{"aout"};
    static const constexpr midi_in midi_ins[]{"min"};
    static const constexpr midi_out midi_outs[]{"mout"};

    static const constexpr auto controls = std::make_tuple(
            Control::FloatSlider{"FloatSlider", -10, 20, 5}
          , Control::LogFloatSlider{"LogFloatSlider", -10, 20, 5}
          , Control::FloatKnob{"FloatKnob", -10, 20, 5}
          , Control::LogFloatKnob{"LogFloatKnob", -10, 20, 5}
          , Control::IntSlider{"IntSlider", -10, 20, 5}
          , Control::IntSpinBox{"IntSpinBox", -10, 20, 5}
          , Control::Toggle{"Toggle", true}
          , Control::ChooserToggle{"ChooserToggle", {"false", "true"}, true}
          , Control::LineEdit{"LineEdit", "henlo"}
          , Control::TimeSignatureChooser{"TimeSignatureChooser", "4/4"}
          , Control::Widgets::QuantificationChooser()
          , Control::Widgets::MusicalDurationChooser()
          , Control::Widgets::DurationChooser()
          , Control::Widgets::FreqSlider()
          , Control::Widgets::LFOFreqSlider()
          , Control::Widgets::FreqKnob()
          , Control::Widgets::LFOFreqKnob()
          , Control::Widgets::TimeSigChooser()
          , Control::Widgets::WaveformChooser()
          );
  };

  using control_policy = ossia::safe_nodes::default_tick;
  template<typename... Args>
  static void run(Args&&...)
  {
  }
};
}

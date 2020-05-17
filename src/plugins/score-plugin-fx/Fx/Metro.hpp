#pragma once
#include <Engine/Node/PdNode.hpp>

#include <numeric>
namespace Nodes
{
namespace Metro
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Free metronome";
    static const constexpr auto objectKey = "Metro";
    static const constexpr auto category = "Control";
    static const constexpr auto author = "ossia score";
    static const constexpr auto kind = Process::ProcessCategory::Generator;
    static const constexpr auto description
        = "Metronome which is not synced to the parent quantization settings";

    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid = make_uuid("50439197-521E-4ED0-A3B7-EDD8DEAEAC93");

    static const constexpr value_out value_outs[]{"out"};

    static const constexpr auto controls = std::make_tuple(
        Control::Widgets::MusicalDurationChooser(),
        Control::Widgets::LFOFreqSlider(),
        Control::ChooserToggle{"Quantify", {"Free", "Sync"}, false});
  };

  static constexpr int64_t
  get_period(bool use_tempo, double quantif, double freq, double tempo, int sr)
  {
    if (use_tempo)
    {
      const auto whole_dur = 240. / tempo;
      const auto whole_samples = whole_dur * sr;
      return quantif * whole_samples;
    }
    else
    {
      return sr / freq;
    }
  }

  static constexpr ossia::time_value next_date(ossia::time_value cur_date, int64_t period)
  {
    return ossia::time_value{(int64_t)(period * (1 + cur_date.impl / period))};
  }

  using control_policy = ossia::safe_nodes::last_tick;
  static void
  run(float quantif,
      float freq,
      bool val,
      ossia::value_port& res,
      ossia::token_request tk,
      ossia::exec_state_facade st)
  {
    if (tk.date > tk.prev_date)
    {
      const auto period = get_period(val, quantif, freq, tk.tempo, st.sampleRate());
      const auto next = next_date(tk.prev_date, period);
      if (tk.in_range(next))
      {
        res.write_value(ossia::impulse{}, tk.to_physical_time_in_tick(next, st.modelToSamples()));
      }
    }
  }
};
}
}

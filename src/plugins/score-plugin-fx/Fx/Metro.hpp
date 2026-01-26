#pragma once
#include <Fx/Types.hpp>

#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace Nodes
{
namespace Metro
{
struct Node
{
  halp_meta(name, "Free metronome")
  halp_meta(c_name, "Metro")
  halp_meta(category, "Timing/Control")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/control-utilities.html#free-metronome")
  halp_meta(description, "Metronome which is not synced to the parent quantization settings")

  halp_meta(uuid, "50439197-521E-4ED0-A3B7-EDD8DEAEAC93")

  struct
  {
    musical_duration_selector<"Duration"> dur;
    halp::log_hslider_f32<"Frequency", halp::range{0.01f, 100.f, 1.f}> frequency;
    struct : halp::toggle<"Quantify">
    {
      struct range
      {
        std::array<std::string_view, 2> values{"Free", "Sync"};
        int init = 0;
      };
    } quantify;
  } inputs;
  struct
  {
    halp::timed_callback<"out"> out;

  } outputs;

  static constexpr int64_t get_period(bool use_tempo, double quantif, double freq, double tempo, int sr)
  {
    if(use_tempo)
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

  static constexpr int64_t next_date(int64_t cur_date, int64_t period)
  {
    return (int64_t)(period * (cur_date / period));
  }

  halp::setup setup;
  void prepare(halp::setup s) { setup = s; }

  //using control_policy = ossia::safe_nodes::last_tick;
  using tick = halp::tick_musical;
  void operator()(const halp::tick_musical& tk)
  {
    if(tk.start_position_in_quarters < tk.end_position_in_quarters)
    {
      // in samples:
      const auto period = get_period(
          inputs.quantify, inputs.dur.value, inputs.frequency, tk.tempo, setup.rate);
      if(period <= 0)
        return;

      const auto next = next_date(tk.position_in_frames, period);
      if(tk.in_range(next))
      {
        outputs.out(next - tk.position_in_frames);
      }
    }
  }
};
}
}

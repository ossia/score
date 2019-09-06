#pragma once
#include <Engine/Node/PdNode.hpp>

#include <numeric>
namespace Nodes
{
namespace ClassicalBeat
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Classical Beat";
    static const constexpr auto objectKey = "ClassicalBeat";
    static const constexpr auto category = "Control";
    static const constexpr auto author = "ossia score";
    static const constexpr auto kind = Process::ProcessCategory::Generator;
    static const constexpr auto description
        = "A simple metronome - currently only supports 3/4 and 4/4";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid
        = make_uuid("5d71816e-e081-4d85-b385-fb42472b41bf");

    static const constexpr value_out value_outs[]{"out"};

    static const constexpr auto controls = std::make_tuple(
        Control::Widgets::TempoChooser(),
        Control::Widgets::TimeSigChooser());
  };

  static constexpr int64_t get_period(double quantif, double tempo, int sr)
  {
    const auto whole_dur = 240. / tempo;
    const auto whole_samples = whole_dur * sr;
    return quantif * whole_samples;
  }

  static constexpr ossia::time_value
  next_date(ossia::time_value cur_date, int64_t period)
  {
    return ossia::time_value{period * (1 + cur_date / period)};
  }

  using control_policy = ossia::safe_nodes::last_tick;
  static void
  run(float tempo,
      const Control::time_signature& sig,
      ossia::value_port& res,
      ossia::token_request tk,
      ossia::exec_state_facade st)
  {
    if (tk.date > tk.prev_date)
    {
      if (tk.prev_date == 0)
      {
        res.write_value(1, tk.tick_start());
      }

      const auto period
          = get_period(1. / sig.lower, (double)tempo, st.sampleRate());
      const auto next = next_date(tk.prev_date, period);
      if (next.impl < tk.date.impl)
      {
        if (sig == Control::time_signature{3, 4})
        {
          ossia::timed_value t;
          if (next.impl == 0 || (next.impl % (3 * period)) == 0)
            res.write_value(1, tk.to_tick_time(next));
          else
            res.write_value(0, tk.to_tick_time(next));
        }
        else if (sig == Control::time_signature{4, 4})
        {
          ossia::timed_value t;
          if (next.impl == 0 || next.impl % (2 * period) == 0)
            res.write_value(1, tk.to_tick_time(next));
          else
            res.write_value(0, tk.to_tick_time(next));
        }
      }
    }
  }
};
}
}

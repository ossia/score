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
    static const constexpr auto prettyName = "Impulse Metronome";
    static const constexpr auto objectKey = "ImpulseMetronome";
    static const constexpr auto category = "Control";
    static const constexpr auto author = "ossia score";
    static const constexpr auto kind = Process::ProcessCategory::Generator;
    static const constexpr auto description
        = "A simple metronome - outputs a bang on the current tick";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid
        = make_uuid("1c185139-04f9-492f-8b4a-000dd4428990");

    static const constexpr value_out value_outs[]{"out"};
  };

  using control_policy = ossia::safe_nodes::last_tick;
  static void
  run(ossia::value_port& res,
      ossia::token_request tk,
      ossia::exec_state_facade st)
  {
    using namespace ossia;
    if (tk.forward())
    {
      if((tk.musical_end_last_bar != tk.musical_start_last_bar) || tk.prev_date == 0_tv)
      {
        // There is a bar change in this tick, start the hi sound
        double musical_tick_duration = tk.musical_end_position - tk.musical_start_position;
        double musical_bar_start = tk.musical_end_last_bar - tk.musical_start_position;
        int64_t samples_tick_duration = tk.physical_write_duration(st.modelToSamples());
        if(samples_tick_duration > 0)
        {
          double ratio = musical_bar_start / musical_tick_duration;
          const int64_t hi_start_sample = samples_tick_duration * ratio;
          res.write_value(ossia::impulse{}, hi_start_sample);
        }
      }
      else
      {
        int64_t start_quarter = std::floor(tk.musical_start_position - tk.musical_start_last_bar);
        int64_t end_quarter = std::floor(tk.musical_end_position - tk.musical_start_last_bar);
        if(start_quarter != end_quarter)
        {
          // There is a quarter change in this tick, start the lo sound
          // start_position is prev_date
          // end_position is date
          double musical_tick_duration = tk.musical_end_position - tk.musical_start_position;
          double musical_bar_start = (end_quarter + tk.musical_start_last_bar) - tk.musical_start_position;
          int64_t samples_tick_duration = tk.physical_write_duration(st.modelToSamples());
          if(samples_tick_duration > 0)
          {
            double ratio = musical_bar_start / musical_tick_duration;
            const int64_t lo_start_sample = samples_tick_duration * ratio;
            res.write_value(ossia::impulse{}, lo_start_sample);
          }
        }
      }
    }

  }
};
}
}

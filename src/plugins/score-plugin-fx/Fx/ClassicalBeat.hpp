#pragma once
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace Nodes
{
namespace ClassicalBeat
{
struct Node
{
  halp_meta(name, "Beat Metronome")
  halp_meta(c_name, "ImpulseMetronome")
  halp_meta(category, "Timing/Control")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/control-utilities.html#impulse-metronome")
  halp_meta(description, "A simple metronome - outputs a bang on the current tick")
  halp_meta(uuid, "1c185139-04f9-492f-8b4a-000dd4428990")
  halp_flag(deprecated); // use TimingSplitter instead

  struct
  {
    halp::timed_callback<"out"> out;
  } outputs;

  using tick = halp::tick_musical;
  void operator()(const halp::tick_musical& tk)
  {
    using namespace ossia;
    if(tk.start_position_in_quarters < tk.end_position_in_quarters)
    {
      const auto f = [&](int64_t start_sample) { outputs.out(start_sample); };
      tk.metronome(f, f);
    }
  }
};
}
}

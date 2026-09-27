#pragma once
#include <Fx/Types.hpp>

#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <cmath>

namespace Nodes::Metro::v2
{
struct Node
{
  halp_meta(name, "Free metronome")
  halp_meta(c_name, "Metro")
  halp_meta(category, "Timing/Control")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/control-utilities.html#free-metronome")
  halp_meta(description, "Metronome which is not synced to the parent quantization settings")

  halp_meta(uuid, "6e001674-d4a5-478f-9f7e-d7ad1bdcee2b")

  struct
  {
    //! Between two ticks: in seconds, or synced to a note value, where the
    //! ticks fall on the musical grid.
    halp::time_chooser<"Period", halp::range{0.001, 60., 0.25}> period;
  } inputs;
  struct
  {
    halp::timed_callback<"out"> out;
  } outputs;

  halp::setup setup;
  void prepare(halp::setup s) { setup = s; }

  using tick = halp::tick_musical;
  void operator()(const halp::tick_musical& tk)
  {
    if(tk.start_position_in_quarters >= tk.end_position_in_quarters || tk.frames <= 0)
      return;
    const double period = inputs.period.value;
    if(!(period > 0.))
      return;

    if(inputs.period.sync)
    {
      // On the musical grid: every period, counted in quarter notes from the
      // start of the timeline.
      const double q0 = tk.start_position_in_quarters;
      const double q1 = tk.end_position_in_quarters;
      const double step = period * tk.tempo / 60.;
      if(!(step > 0.))
        return;
      // Grid point k is k * step in every tick, computed the same way, so that
      // the one sitting on the boundary of two ticks goes out exactly once.
      auto k = int64_t(std::ceil(q0 / step));
      while(double(k - 1) * step >= q0)
        --k;
      while(double(k) * step < q0)
        ++k;
      for(double q = double(k) * step; q < q1; q = double(++k) * step)
        outputs.out(std::min<int64_t>(
            tk.frames - 1, int64_t((q - q0) / (q1 - q0) * tk.frames)));
    }
    else
    {
      // Every period of samples since playback started.
      const int64_t p = std::max<int64_t>(1, std::llround(period * setup.rate));
      const int64_t pos = tk.position_in_frames;
      for(int64_t t = ((pos + p - 1) / p) * p; t < pos + tk.frames; t += p)
        outputs.out(t - pos);
    }
  }
};
}

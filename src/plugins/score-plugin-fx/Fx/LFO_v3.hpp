#pragma once
#include <Fx/LFO_v2.hpp>
#include <Fx/Types.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/detail/math.hpp>

#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <rnd/random.hpp>
#include <tuplet/tuple.hpp>

#include <random>

namespace Nodes::LFO::v3
{
//! The waveforms. The stepped ones come in two kinds: sent only when the step
//! changes, or at every tick.
enum Waveform
{
  Sin,
  Triangle,
  Saw,
  RampDown,
  Square,
  SquareOnChange,
  SampleAndHoldEveryTick,
  SampleAndHold,
  Noise1,
  Noise2,
  Noise3,
  Drift
};

struct Node
{
  halp_meta(name, "LFO")
  halp_meta(c_name, "LFO")
  halp_meta(category, "Control/Generators")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/lfo.html#lfo")
  halp_meta(description, "Low-frequency oscillator")
  halp_meta(recommended_height, 130.)
  halp_meta(uuid, "0b1b1816-c33e-4796-a16d-5aab27fe600f");

  struct ins
  {
    //! One cycle: in seconds, or synced to a note value.
    halp::time_chooser<"Period", halp::range{0.01, 60., 1.}> period;
    //! 0.5 is the plain waveform. Sine, triangle: where the peak falls (the
    //! share of the cycle spent rising). Square, sample and hold: the share of
    //! the cycle before the second step (pulse width). Saw, ramp down: the curve.
    //! Drift: how rough the wandering is.
    halp::knob_f32<"Shape", halp::range{0., 1., 0.5}> shape;
    //! Back to the start of the cycle.
    halp::impulse_button<"Retrigger"> retrigger;
    halp::knob_f32<"Ampl.", halp::range{0., 2., 0.5}> ampl;
    halp::knob_f32<"Offset", halp::range{-1., 1., 0.5}> offset;
    //! In degrees: each tick the phase moves by a random amount within
    //! +/- this much. 180 is anywhere in the cycle.
    halp::knob_f32<"Jitter", halp::range{0., 180., 0.}> jitter;
    //! In degrees: how far into its cycle the waveform starts (at the start of
    //! playback, on Retrigger, on the bar when locked), a constant offset from
    //! there on. 0 and 360 are the same phase; values beyond the range wrap.
    halp::knob_f32<"Phase", halp::range{0., 360., 0.}> phase;
    struct : halp::enum_t<Waveform, "Waveform">
    {
      static constexpr auto pixmaps()
      {
        return std::array<const char*, 24>{
            ":/icons/wave_sin_off.png",
            ":/icons/wave_sin_on.png",
            ":/icons/wave_triangle_off.png",
            ":/icons/wave_triangle_on.png",
            ":/icons/wave_saw_off.png",
            ":/icons/wave_saw_on.png",
            ":/icons/wave_ramp_down_off.png",
            ":/icons/wave_ramp_down_on.png",
            ":/icons/wave_square_ticks_off.png",
            ":/icons/wave_square_ticks_on.png",
            ":/icons/wave_square_changes_off.png",
            ":/icons/wave_square_changes_on.png",
            ":/icons/wave_sample_and_hold_ticks_off.png",
            ":/icons/wave_sample_and_hold_ticks_on.png",
            ":/icons/wave_sample_and_hold_changes_off.png",
            ":/icons/wave_sample_and_hold_changes_on.png",
            ":/icons/wave_noise1_off.png",
            ":/icons/wave_noise1_on.png",
            ":/icons/wave_noise2_off.png",
            ":/icons/wave_noise2_on.png",
            ":/icons/wave_noise3_off.png",
            ":/icons/wave_noise3_on.png",
            ":/icons/wave_drift_off.png",
            ":/icons/wave_drift_on.png"};
      }
    } waveform;
    //! Synced only. Off, the cycles count from where playback started or the
    //! retrigger was pressed; on, from the start of the timeline (or the
    //! retrigger), so they fall on the bars wherever playback starts.
    halp::toggle<"Lock to bars"> lock_to_bars;
  } inputs;
  struct
  {
    // FIXME output range ???
    struct : halp::val_port<"Out", std::optional<float>>
    {
      struct range
      {
        float min = 0.;
        float max = 1.;
      };
    } out;
  } outputs;

  //! Counted cycles (free-running, or synced and not locked to the bars):
  //! where the cycle is, in [0; 1), and how many whole cycles went by (the
  //! drift's position, which must not repeat).
  double phase{};
  int64_t cycles{};
  //! Locked to the bars: the musical position, in quarters, the cycles are
  //! read from.
  double sync_origin{};
  rnd::pcg rd{random_source()};
  uint32_t seed{uint32_t(rd())};

  // The stepped waveforms: the value held, and whether it was already sent.
  float held{};
  int last_square{}; // -1, +1, or 0 before the first tick
  bool held_valid{};

  static double frac(double x) noexcept { return x - std::floor(x); }

  //! Sine and triangle, x in [0; 1) through the cycle, 0 rising at x = 0 as
  //! sin() is; `rise` of the cycle goes from the trough to the peak. The warp
  //! is at the peak and the trough, where a sine is flat: no kink.
  static double skewed_position(double x, double rise) noexcept
  {
    const double r = std::clamp(rise, 0.02, 0.98);
    const double u = frac(x + 0.25); // 0 at the trough
    return u < r ? 0.5 * u / r : 0.5 + 0.5 * (u - r) / (1. - r);
  }
  static double sine(double x, double rise) noexcept
  {
    return -std::cos(ossia::two_pi * skewed_position(x, rise));
  }
  static double triangle(double x, double rise) noexcept
  {
    const double w = skewed_position(x, rise); // 0 trough, 0.5 peak
    return w < 0.5 ? -1. + 4. * w : 3. - 4. * w;
  }
  //! One ramp a cycle, 0 at x = 0; bent by the shape: linear at 0.5.
  static double saw(double x, double shape) noexcept
  {
    const double e = std::exp2((0.5 - std::clamp(shape, 0., 1.)) * 4.); // 4 .. 1/4
    return 2. * std::pow(frac(x + 0.5), e) - 1.;
  }
  //! The two steps of a cycle: the first until `duty`, the second after.
  static int step(double x, double duty) noexcept
  {
    return frac(x) < std::clamp(duty, 0., 1.) ? 1 : -1;
  }

  //! A random value in [-1; 1] for each integer, the same every time.
  static float lattice(int64_t i, uint32_t seed) noexcept
  {
    uint64_t h = uint64_t(i) * 0x9E3779B97F4A7C15ull ^ (uint64_t(seed) << 17);
    h ^= h >> 31;
    h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 29;
    return float(h >> 40) / float(1 << 23) - 1.f;
  }

  //! Smooth value noise: one wander per cycle, finer detail by `roughness`.
  static float drift(double cycles, double roughness, uint32_t seed) noexcept
  {
    auto octave = [&](double t, uint32_t sd) {
      const double f = std::floor(t);
      const double u = t - f;
      const double k = u * u * (3. - 2. * u); // smoothstep
      const float a = lattice(int64_t(f), sd), b = lattice(int64_t(f) + 1, sd);
      return a + (b - a) * float(k);
    };
    const double r = std::clamp(roughness, 0., 1.);
    const float v = octave(cycles, seed) + float(r) * 0.5f * octave(cycles * 2.3, seed ^ 0x5bd1e995u)
                    + float(r * r) * 0.25f * octave(cycles * 5.1, seed ^ 0x27d4eb2du);
    return std::clamp(v / float(1. + 0.5 * r + 0.25 * r * r), -1.f, 1.f);
  }

  using tick = halp::tick_flicks;
  void operator()(const halp::tick_flicks& tk)
  {
    const auto ampl = inputs.ampl.value;
    const auto offset = inputs.offset.value;
    const auto jitter = inputs.jitter.value;
    const auto type = inputs.waveform.value;
    const double shape = inputs.shape.value;
    const double period = std::max(1e-4, (double)inputs.period.value);
    const bool sync = inputs.period.sync;
    const bool locked = sync && inputs.lock_to_bars.value;

    if(inputs.retrigger)
    {
      phase = 0.;
      cycles = 0;
      sync_origin = tk.start_position_in_quarters;
      held_valid = false;
      last_square = 0;
      seed = uint32_t(rd());
    }
    else if(sync && !locked && tk.unexpected_bar_change())
    {
      phase = 0.;
      cycles = 0;
    }

    // Cycles elapsed at the start and at the end of the tick. Locked to the
    // bars, they are read off the musical position: the cycles stay on the
    // bars through tempo changes, jumps and loops, with no drift from
    // accumulating. Otherwise they accumulate from the start, synced in
    // quarters. The binding hands a synced period over in seconds at the
    // current tempo: back to quarter notes.
    const double quarters_per_cycle = period * tk.tempo / 60.;
    double c0{}, c1{};
    if(locked)
    {
      if(quarters_per_cycle > 0.)
      {
        c0 = (tk.start_position_in_quarters - sync_origin) / quarters_per_cycle;
        c1 = (tk.end_position_in_quarters - sync_origin) / quarters_per_cycle;
      }
    }
    else
    {
      c0 = double(cycles) + phase;
      if(!sync)
        c1 = c0
             + double(tk.model_read_duration())
                   / (period * ossia::flicks_per_second<double>);
      else if(quarters_per_cycle > 0.)
        c1 = c0
             + (tk.end_position_in_quarters - tk.start_position_in_quarters)
                   / quarters_per_cycle;
      else
        c1 = c0;
    }

    double shift = inputs.phase.value;
    if(jitter > 0)
      shift += std::uniform_real_distribution<float>(-jitter, jitter)(this->rd);
    shift /= 360.;
    c0 += shift;
    c1 += shift;
    const double x = frac(c0);

    // Every waveform, the noises included, spans [-1; 1]: Ampl. and Offset
    // place them all the same way.
    const auto add_val
        = [&](auto new_val) { outputs.out.value = ampl * new_val + offset; };

    // Sample and hold: a new value at each of the two steps of a cycle.
    const auto crossed = [&] {
      return std::floor(c0) != std::floor(c1) || step(c0, shape) != step(c1, shape);
    };

    switch(type)
    {
      case Sin:
        add_val(sine(x, shape));
        break;
      case Triangle:
        add_val(triangle(x, shape));
        break;
      case Saw:
        add_val(saw(x, shape));
        break;
      case RampDown:
        add_val(-saw(x, shape));
        break;
      case Square:
        add_val(float(step(x, shape)));
        break;
      case SquareOnChange: {
        const int s = step(x, shape);
        if(s != last_square)
          add_val(float(s));
        last_square = s;
        break;
      }
      case SampleAndHold:
        if(!held_valid || crossed())
        {
          held = std::uniform_real_distribution<float>(-1.f, 1.f)(this->rd);
          held_valid = true;
          add_val(held);
        }
        break;
      case SampleAndHoldEveryTick:
        if(!held_valid || crossed())
        {
          held = std::uniform_real_distribution<float>(-1.f, 1.f)(this->rd);
          held_valid = true;
        }
        add_val(held);
        break;
      case Noise1:
        add_val(std::uniform_real_distribution<float>(-1.f, 1.f)(this->rd));
        break;
      case Noise2:
        add_val(std::clamp(std::normal_distribution<float>(0.f, 0.4f)(this->rd), -1.f, 1.f));
        break;
      case Noise3:
        add_val(std::clamp(std::cauchy_distribution<float>(0.f, 0.1f)(this->rd), -1.f, 1.f));
        break;
      case Drift:
        add_val(drift(c0, shape, seed));
        break;
    }

    // Whole cycles move to the counter, so the phase keeps its precision
    // however long it runs, and in either direction. Locked, the count follows
    // the position so that unlocking carries on from there; locking snaps the
    // cycle to the bars.
    const double end = locked ? c1 - shift : phase + (c1 - c0);
    const double whole = std::floor(end);
    phase = end - whole;
    cycles = locked ? int64_t(whole) : cycles + int64_t(whole);
  }

  struct ui
  {
    // Three columns of the same height: the waveforms, the knobs two rows
    // deep, the toggles.
    halp_meta(layout, halp::layouts::hbox)
    struct
    {
      halp_meta(layout, halp::layouts::vbox)
      halp_meta(background, halp::colors::background_mid)
      halp::control<&ins::waveform> w;
    } shape;

    struct
    {
      halp_meta(layout, halp::layouts::vbox)
      halp_meta(background, halp::colors::background_mid)
      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        halp::control<&ins::period> t;
        halp::control<&ins::shape> s;
        halp::control<&ins::ampl> a;
      } top;
      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        halp::control<&ins::offset> o;
        halp::control<&ins::jitter> j;
        halp::control<&ins::phase> p;
      } bottom;
    } knobs;

    struct
    {
      halp_meta(layout, halp::layouts::vbox)
      halp_meta(background, halp::colors::background_mid)
      halp::control<&ins::retrigger> r;
      halp::control<&ins::lock_to_bars> l;
    } toggles;
  };
};
}

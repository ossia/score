#pragma once
#include <Fx/Types.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/detail/math.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <rnd/random.hpp>
#include <tuplet/tuple.hpp>

#include <random>

namespace Nodes
{

namespace LFO
{
static inline std::random_device& random_source()
{
  static thread_local std::random_device d;
  return d;
}

namespace v1
{
struct Node
{
  halp_meta(name, "LFO (old)")
  halp_meta(c_name, "LFO")
  halp_meta(category, "Control/Generators")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/lfo.html#lfo")
  halp_meta(description, "Low-frequency oscillator")
  halp_flag(deprecated);
  halp_meta(recommended_height, 130.)
  halp_meta(uuid, "0697b807-f588-49b5-926c-f97701edd0d8");

  struct Inputs
  {
    // FIXME knob
    halp::log_hslider_f32<"Freq.", halp::range{0.01f, 100.f, 1.f}> freq;
    halp::knob_f32<"Ampl.", halp::range{0., 1000., 0.}> ampl;
    halp::knob_f32<"Fine", halp::range{0., 1., 0.5}> ampl_fine;
    halp::knob_f32<"Offset", halp::range{-1000., 1000., 0.}> offset;
    halp::knob_f32<"Fine", halp::range{-1., 1., 0.5}> offset_fine;
    halp::knob_f32<"Jitter", halp::range{0., 1., 0}> jitter;
    halp::knob_f32<"Phase", halp::range{-1., 1., 0.}> phase;
    halp::enum_t<Control::Widgets::Waveform, "Waveform"> waveform;
    quant_selector<"Quantification"> quant;
  } inputs;
  struct
  {
    halp::val_port<"Out", std::optional<float>> out;
  } outputs;

  double phase{};
  rnd::pcg rd;

  using tick = halp::tick_flicks;
  void operator()(const halp::tick_flicks& tk)
  {
    constexpr const double sine_ratio = ossia::two_pi / ossia::flicks_per_second<double>;
    const auto elapsed = tk.model_read_duration();

    const auto quantif = inputs.quant.value;
    auto freq = inputs.freq.value;
    auto ampl = inputs.ampl.value;
    const auto ampl_fine = inputs.ampl_fine.value;
    auto offset = inputs.offset.value;
    const auto offset_fine = inputs.offset_fine.value;
    const auto jitter = inputs.jitter.value;
    auto custom_phase = inputs.phase.value;
    const auto type = inputs.waveform.value;

    if(quantif)
    {
      // Determine the frequency with the quantification
      if(tk.unexpected_bar_change())
      {
        this->phase = 0;
      }

      // If quantif == 1, we quantize to the bar
      //   => f = 0.5 hz
      // If quantif == 1/4, we quantize to the quarter
      //   => f = 2hz
      // -> sin(elapsed * freq * 2 * pi / fps)
      // -> sin(elapsed * 4 * 2 * pi / fps)
      freq = 1. / (2. * quantif);
    }

    const auto ph_delta = elapsed * freq * sine_ratio;

    {
      auto ph = this->phase;
      if(jitter > 0)
      {
        ph += std::normal_distribution<float>(0., 0.25)(this->rd) * jitter;
      }

      ampl += ampl_fine;
      offset += offset_fine;

      using namespace Control::Widgets;

      const auto add_val
          = [&](auto new_val) { outputs.out.value = ampl * new_val + offset; };

      custom_phase = custom_phase * ossia::pi;
      switch(type)
      {
        case Sin:
          add_val(std::sin(custom_phase + ph));
          break;
        case Triangle:
          add_val(std::asin(std::sin(custom_phase + ph)) / ossia::half_pi);
          break;
        case Saw:
          add_val(std::atan(std::tan(custom_phase + ph)) / ossia::half_pi);
          break;
        case Square:
          add_val((std::sin(custom_phase + ph) > 0.f) ? 1.f : -1.f);
          break;
        case SampleAndHold: {
          const auto start_s = std::sin(custom_phase + ph);
          const auto end_s = std::sin(custom_phase + ph + ph_delta);
          if((start_s > 0 && end_s <= 0) || (start_s <= 0 && end_s > 0))
          {
            add_val(std::uniform_real_distribution<float>(-1.f, 1.f)(this->rd));
          }
          break;
        }
        case Noise1:
          add_val(std::uniform_real_distribution<float>(-1.f, 1.f)(this->rd));
          break;
        case Noise2:
          add_val(std::normal_distribution<float>(0.f, 1.f)(this->rd));
          break;
        case Noise3:
          add_val(
              std::clamp(std::cauchy_distribution<float>(0.f, 1.f)(this->rd), 0.f, 1.f));
          break;
      }
    }

    this->phase += ph_delta;
  }
};
}
}
}

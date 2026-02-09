#pragma once
#include <Fx/LFO.hpp>
#include <Fx/Types.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/detail/math.hpp>

#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <rnd/random.hpp>
#include <tuplet/tuple.hpp>

#include <numbers>
#include <random>

namespace Nodes::LFO::v2
{
struct Node
{
  halp_meta(name, "LFO")
  halp_meta(c_name, "LFO")
  halp_meta(category, "Control/Generators")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/lfo.html#lfo")
  halp_meta(description, "Low-frequency oscillator")
  halp_meta(recommended_height, 130.)
  halp_meta(uuid, "1e17e479-3513-44c8-a8a7-017be9f6ac8a");

  struct ins
  {
    halp::log_hslider_f32<"Freq.", halp::range{0.01f, 100.f, 1.f}> freq;
    halp::knob_f32<"Ampl.", halp::range{0., 2., 0.5}> ampl;
    halp::knob_f32<"Offset", halp::range{-1., 1., 0.5}> offset;
    halp::knob_f32<"Jitter", halp::range{0., 1., 0}> jitter;
    halp::knob_f32<"Phase", halp::range{-1., 1., 0.}> phase;
    struct : halp::enum_t<Control::Widgets::Waveform, "Waveform">
    {
      static constexpr auto pixmaps()
      {
        return std::array<const char*, 16>{
            ":/icons/wave_sin_off.png",
            ":/icons/wave_sin_on.png",
            ":/icons/wave_triangle_off.png",
            ":/icons/wave_triangle_on.png",
            ":/icons/wave_saw_off.png",
            ":/icons/wave_saw_on.png",
            ":/icons/wave_square_off.png",
            ":/icons/wave_square_on.png",
            ":/icons/wave_sample_and_hold_off.png",
            ":/icons/wave_sample_and_hold_on.png",
            ":/icons/wave_noise1_off.png",
            ":/icons/wave_noise1_on.png",
            ":/icons/wave_noise2_off.png",
            ":/icons/wave_noise2_on.png",
            ":/icons/wave_noise3_off.png",
            ":/icons/wave_noise3_on.png"};
      }
    } waveform;
    quant_selector<"Quantification"> quant;

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

  double phase{};
  rnd::pcg rd{random_source()};

  using tick = halp::tick_flicks;
  void operator()(const halp::tick_flicks& tk)
  {
    constexpr const double sine_ratio = ossia::two_pi / ossia::flicks_per_second<double>;
    const auto elapsed = tk.model_read_duration();

    const auto quantif = inputs.quant.value;
    auto freq = inputs.freq.value;
    auto ampl = inputs.ampl.value;
    auto offset = inputs.offset.value;
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
      auto ph = this->phase * std::numbers::pi;
      if(jitter > 0)
      {
        ph += std::normal_distribution<float>(0., 0.25)(this->rd) * jitter;
      }

      using namespace Control::Widgets;

      const auto add_val
          = [&](auto new_val) { outputs.out.value = ampl * new_val + offset; };
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

  struct ui
  {
    halp_meta(layout, halp::layouts::hbox)
    struct
    {
      halp_meta(layout, halp::layouts::vbox)
      halp_meta(background, halp::colors::background_mid)
      struct
      {
        halp_meta(layout, halp::layouts::hbox)

        halp::control<&ins::freq> f;
        halp::control<&ins::quant> q;
      } timing;
      halp::control<&ins::waveform> w;
    } gen;

    struct
    {
      halp_meta(layout, halp::layouts::vbox)
      halp_meta(background, halp::colors::background_mid)
      halp::control<&ins::ampl> a;
      halp::control<&ins::offset> o;
    } ampl;
    struct
    {
      halp_meta(layout, halp::layouts::vbox)
      halp_meta(background, halp::colors::background_mid)
      halp::control<&ins::jitter> j;
      halp::control<&ins::phase> p;
    } modulation;
  };
};
}

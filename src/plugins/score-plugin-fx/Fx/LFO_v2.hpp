#pragma once
#include <Fx/LFO.hpp>
#include <Fx/Types.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/detail/math.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <rnd/random.hpp>
#include <tuplet/tuple.hpp>

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

  struct
  {
    halp::log_hslider_f32<"Freq.", halp::range{0.01f, 100.f, 1.f}> freq;
    halp::knob_f32<"Ampl.", halp::range{0., 2., 0.5}> ampl;
    halp::knob_f32<"Offset", halp::range{-1., 1., 0.5}> offset;
    halp::knob_f32<"Jitter", halp::range{0., 1., 0}> jitter;
    halp::knob_f32<"Phase", halp::range{-1., 1., 0.}> phase;
    halp::enum_t<Control::Widgets::Waveform, "Waveform"> waveform;
    quant_selector<"Quantification"> quant;

  } inputs;
  struct
  {
    // FIXME output range ???
    halp::val_port<"Out", std::optional<float>> out;
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
      auto ph = this->phase;
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

#if FX_UI
  static void item(
      Process::LogFloatSlider& freq, Process::FloatKnob& ampl,
      Process::FloatKnob& offset, Process::FloatKnob& jitter, Process::FloatKnob& phase,
      Process::Enum& type, Process::ComboBox& quantif,
      const Process::ProcessModel& process, QGraphicsItem& parent, QObject& context,
      const Process::Context& doc)
  {
    using namespace Process;
    using namespace std;
    using namespace tuplet;
    const Process::PortFactoryList& portFactory
        = doc.app.interfaces<Process::PortFactoryList>();
    const auto h = 60;
    const auto w = 50;

    const auto c0 = 10;
    const auto c1 = 180;
    const auto c2 = 230;

    auto c0_bg = new score::BackgroundItem{&parent};
    c0_bg->setRect({0., 0., 170., 130.});
    auto c1_bg = new score::BackgroundItem{&parent};
    c1_bg->setRect({170., 0., 100., 130.});
    auto c2_bg = new score::BackgroundItem{&parent};
    c2_bg->setRect({270., 0., 60., 130.});

    auto freq_item = makeControl(
        get<0>(Metadata::controls), freq, parent, context, doc, portFactory);
    freq_item.root.setPos(c0, 0);

    auto ampl_item = makeControl(
        get<1>(Metadata::controls), ampl, parent, context, doc, portFactory);
    ampl_item.root.setPos(c1, 0);

    auto offset_item = makeControl(
        get<2>(Metadata::controls), offset, parent, context, doc, portFactory);
    offset_item.root.setPos(c1, h);

    auto jitter_item = makeControl(
        get<3>(Metadata::controls), jitter, parent, context, doc, portFactory);
    jitter_item.root.setPos(c2 + w, 0);

    auto phase_item = makeControl(
        get<4>(Metadata::controls), phase, parent, context, doc, portFactory);
    phase_item.root.setPos(c2 + w, h);

    auto type_item = makeControlNoText(
        get<5>(Metadata::controls), type, parent, context, doc, portFactory);
    type_item.root.setPos(c0, h);
    type_item.control.rows = 2;
    type_item.control.columns = 4;
    type_item.control.setRect(QRectF{0, 0, 104, 44});
    type_item.control.setPos(10, 0);
    type_item.port.setPos(0, 17);

    auto quant_item = makeControlNoText(
        get<6>(Metadata::controls), quantif, parent, context, doc, portFactory);
    quant_item.root.setPos(90, 25);
    quant_item.port.setPos(-10, 2);
  }
#endif

  // With metaclasses, we should instead create structs with named members but
  // where types are either controls, ports, items, inlets...
};
}

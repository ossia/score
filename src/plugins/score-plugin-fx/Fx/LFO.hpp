#pragma once
#include <ossia/detail/math.hpp>
#include <ossia/detail/flicks.hpp>

#include <Engine/Node/PdNode.hpp>

#include <array>
#include <bitset>
#include <map>
#include <random>
#include <tuple>
#include <utility>

#include <rnd/random.hpp>
/*
template<typename Node>
struct Spec
{
  using info = ossia::safe_nodes::info_functions<Node>;
  ossia::safe_nodes::safe_node<Node>& node;

  template<std::size_t N, typename T, typename M>
  const auto& get(typename T::Controls)
  {
    return node.get_control_accessor<N + info::control_start>(node.inputs());
  }
  template<std::size_t N, typename T, typename M>
  const ossia::value_port& get(typename T::ValueIns)
  {
    return node.inputs()[N + info::audio_in_count + info::midi_in_count];
  }
  template<std::size_t N, typename T, typename M>
  ossia::value_port& get(typename T::ValueOuts)
  {
    return *node.outputs()[N + info::audio_out_count + info::midi_out_count]->data.template target<ossia::value_port>();
  }
  template<std::size_t N, typename T, typename M>
  const ossia::audio_port& get(typename T::AudioIns)
  {
    return node.inputs()[N];
  }
  template<std::size_t N, typename T, typename M>
  ossia::audio_port& get(typename T::AudioOuts)
  {
    return node.outputs()[N];
  }
  template<std::size_t N, typename T, typename M>
  const ossia::midi_port& get(typename T::MidiIns)
  {
    return node.inputs()[N + info::audio_in_count];
  }
  template<std::size_t N, typename T, typename M>
  ossia::midi_port& get(typename T::MidiOuts)
  {
    return node.outputs()[N + info::audio_in_count];
  }
};

#define _get(Kind, Member) spec.get<offsetof(Metadata::Kind, Member), Metadata, decltype(std::declval<Metadata::Kind>().Member)>(Metadata::Kind{})
*/
/* UB:  :'(
template<std::size_t M, typename T>
auto struct_idx()
{
  auto& [a0, a1, a2, a3, a4, a5] = *((T*)nullptr);
  if(M == (std::intptr_t)(char*)&a0) return 0;
  if(M == (std::intptr_t)(char*)&a1) return 1;
  if(M == (std::intptr_t)(char*)&a2) return 2;
  if(M == (std::intptr_t)(char*)&a3) return 3;
  if(M == (std::intptr_t)(char*)&a4) return 4;
  if(M == (std::intptr_t)(char*)&a5) return 5;
  return -1;
}

  static void test(Spec<Node> spec)
  {

    const std::string& type = _get(Controls, waveform);
    ossia::value_port& out = _get(ValueOuts, out);

    return struct_idx<offsetof(foo, c), foo>();
  }

*/
namespace Nodes
{

namespace LFO
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "LFO";
    static const constexpr auto objectKey = "LFO";
    static const constexpr auto category = "Control";
    static const constexpr auto author = "ossia score";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto kind = Process::ProcessCategory::Generator;
    static const constexpr auto description = "Low-frequency oscillator";
    static const constexpr auto uuid
        = make_uuid("0697b807-f588-49b5-926c-f97701edd0d8");

    static const constexpr value_out value_outs[]{"out"};

    static const constexpr auto controls = std::make_tuple(
        Control::Widgets::LFOFreqKnob(),
        Control::FloatKnob{"Ampl.", 0., 1000., 0.},
        Control::FloatKnob{"Fine", 0., 1., 0.5},
        Control::FloatKnob{"Offset", -1000., 1000., 0.},
        Control::FloatKnob{"Fine", -1., 1., 0.5},
        Control::FloatKnob{"Jitter", 0., 1., 0.},
        Control::FloatKnob{"Phase", -1., 1., 0.},
        Control::Widgets::WaveformChooser(),
        Control::Widgets::QuantificationChooser()
          );
  };

  // Idea: save internal state for rewind... ? -> require Copyable
  struct State
  {
    double phase{};
    rnd::pcg rd;
  };

  using control_policy = ossia::safe_nodes::precise_tick;

  static void
  run(
      float freq,
      float ampl,
      float ampl_fine,
      float offset,
      float offset_fine,
      float jitter,
      float custom_phase,
      const std::string& type,
      float quantif,
      ossia::value_port& out,
      ossia::token_request tk,
      ossia::exec_state_facade st,
      State& s)
  {
    constexpr const double sine_ratio = ossia::two_pi / ossia::flicks_per_second<double>;
    const auto& waveform_map = Control::Widgets::waveformMap();
    const auto elapsed = tk.logical_read_duration().impl;

    if(quantif)
    {
      // Determine the frequency with the quantification
      if(tk.unexpected_bar_change())
      {
        s.phase = 0;
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

    if (const auto it = waveform_map.find(type); it != waveform_map.end())
    {
      auto ph = s.phase;
      if (jitter > 0)
      {
        ph += std::normal_distribution<float>(0., 0.25)(s.rd) * jitter;
      }

      ampl += ampl_fine;
      offset += offset_fine;

      using namespace Control::Widgets;

      const auto add_val = [&](auto new_val) {
        out.write_value(ampl * new_val + offset, st.physical_start(tk));
      };
      switch (it->second)
      {
        case Sin:
          add_val(std::sin(custom_phase + ph));
          break;
        case Triangle:
          add_val(std::asin(std::sin(custom_phase + ph)));
          break;
        case Saw:
          add_val(std::atan(std::tan(custom_phase + ph)));
          break;
        case Square:
          add_val((std::sin(custom_phase + ph) > 0.f) ? 1.f : -1.f);
          break;
        case SampleAndHold:
        {
          const auto start_s = std::sin(custom_phase + ph);
          const auto end_s = std::sin(custom_phase + ph + ph_delta);
          if ((start_s > 0 && end_s <= 0) || (start_s <= 0 && end_s > 0))
          {
            add_val(std::uniform_real_distribution<float>(-1., 1.)(s.rd));
          }
          break;
        }
        case Noise1:
          add_val(std::uniform_real_distribution<float>(-1., 1.)(s.rd));
          break;
        case Noise2:
          add_val(std::normal_distribution<float>(0., 1.)(s.rd));
          break;
        case Noise3:
          add_val(std::cauchy_distribution<float>(0., 1.)(s.rd));
          break;
      }
    }

    s.phase += ph_delta;
  }

  static void item(
      Process::LogFloatSlider& freq,
      Process::FloatSlider& ampl,
      Process::FloatSlider& ampl_fine,
      Process::FloatSlider& offset,
      Process::FloatSlider& offset_fine,
      Process::FloatSlider& jitter,
      Process::FloatSlider& phase,
      Process::Enum& type,
      Process::ComboBox& quantif,
      const Process::ProcessModel& process,
      QGraphicsItem& parent,
      QObject& context,
      const Process::Context& doc)
  {
    using namespace Process;
    const Process::PortFactoryList& portFactory = doc.app.interfaces<Process::PortFactoryList>();
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

    auto freq_item = makeControl(std::get<0>(Metadata::controls), freq, parent, context, doc, portFactory);
    freq_item.root.setPos(c0, 0);

    auto quant_item = makeControlNoText(std::get<8>(Metadata::controls), quantif, parent, context, doc, portFactory);
    quant_item.root.setPos(90, 25);
    quant_item.port.setPos(-10, 2);

    auto type_item = makeControlNoText(std::get<7>(Metadata::controls), type, parent, context, doc, portFactory);
    type_item.root.setPos(c0, h);
    type_item.control.rows = 2;
    type_item.control.columns = 4;
    type_item.control.setRect(QRectF{0, 0, 136, 48});
    type_item.control.setPos(10, 0);
    type_item.port.setPos(0, 17);

    auto ampl_item = makeControl(std::get<1>(Metadata::controls), ampl, parent, context, doc, portFactory);
    ampl_item.root.setPos(c1, 0);

    auto ampl_fine_item = makeControl(std::get<2>(Metadata::controls), ampl_fine, parent, context, doc, portFactory);
    ampl_fine_item.root.setPos(c1 + w, 0);

    auto offset_item = makeControl(std::get<3>(Metadata::controls), offset, parent, context, doc, portFactory);
    offset_item.root.setPos(c1, h);

    auto offset_fine_item = makeControl(std::get<4>(Metadata::controls), offset_fine, parent, context, doc, portFactory);
    offset_fine_item.root.setPos(c1 + w, h);

    auto jitter_item = makeControl(std::get<5>(Metadata::controls), jitter, parent, context, doc, portFactory);
    jitter_item.root.setPos(c2 + w, 0);

    auto phase_item = makeControl(std::get<6>(Metadata::controls), phase, parent, context, doc, portFactory);
    phase_item.root.setPos(c2 + w, h);
  }

  // With metaclasses, we should instead create structs with named members but where types are either controls, ports, items, inlets...
};
}
}

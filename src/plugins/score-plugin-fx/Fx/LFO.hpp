#pragma once
#include <ossia/detail/math.hpp>

#include <Engine/Node/PdNode.hpp>

#include <algorithm>
#include <array>
#include <bitset>
#include <map>
#include <random>
#include <tuple>
#include <utility>
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
        Control::Widgets::LFOFreqChooser(),
        Control::FloatSlider{"Ampl.", 0., 1000., 0.},
        Control::FloatSlider{"Fine", 0., 1., 1.},
        Control::FloatSlider{"Offset", -1000., 1000., 0.},
        Control::FloatSlider{"Fine", -1., 1., 0.},
        Control::FloatSlider{"Jitter", 0., 1., 0.},
        Control::FloatSlider{"Phase", -1., 1., 0.},
        Control::Widgets::WaveformChooser());
  };

  // Idea: save internal state for rewind... ? -> require Copyable
  struct State
  {
    int64_t phase{};
    std::mt19937 rd;
  };

  using control_policy = ossia::safe_nodes::precise_tick;

  static void
  run(float freq,
      float ampl,
      float ampl_fine,
      float offset,
      float offset_fine,
      float jitter,
      float phase,
      const std::string& type,
      ossia::value_port& out,
      ossia::token_request tk,
      ossia::exec_state_facade st,
      State& s)
  {
    auto& waveform_map = Control::Widgets::waveformMap();

    if (auto it = waveform_map.find(type); it != waveform_map.end())
    {
      auto ph = s.phase;
      if (jitter > 0)
      {
        ph += std::normal_distribution<float>(0., 5000.)(s.rd) * jitter;
      }

      ampl += ampl_fine;
      offset += offset_fine;

      using namespace Control::Widgets;
      const auto phi
          = phase + (float(ossia::two_pi) * freq * ph) / st.sampleRate();

      auto add_val = [&](auto new_val) {
        out.write_value(ampl * new_val + offset, tk.tick_start());
      };
      switch (it->second)
      {
        case Sin:
          add_val(std::sin(phi));
          break;
        case Triangle:
          add_val(std::asin(std::sin(phi)));
          break;
        case Saw:
          add_val(std::atan(std::tan(phi)));
          break;
        case Square:
          add_val((std::sin(phi) > 0.f) ? 1.f : -1.f);
          break;
        case SampleAndHold:
        {
          auto start_phi
              = phase
                + (float(ossia::two_pi) * freq * s.phase) / st.sampleRate();
          auto end_phi = phase
                         + (float(ossia::two_pi) * freq
                            * (s.phase + tk.date - tk.prev_date))
                               / st.sampleRate();
          auto start_s = std::sin(start_phi);
          auto end_s = std::sin(end_phi);
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

    s.phase += (tk.date - tk.prev_date);
  }


  template<typename C, typename T>
  static auto makeControl(
      C& ctrl,
      T& inlet,
      QGraphicsItem& parent,
      QObject& context,
      const score::DocumentContext& doc,
      const Process::PortFactoryList& portFactory)
  {
    auto item = new score::EmptyRectItem{&parent};

    // Port
    Process::PortFactory* fact = portFactory.get(inlet.concreteKey());
    auto port = fact->makeItem(inlet, doc, item, &context);
    port->setPos(0, 10);

    // Text
    const auto& style = Process::Style::instance();
    auto lab = new score::SimpleTextItem{style.EventWaiting, item};
    lab->setText(ctrl.name);
    lab->setPos(10, 2);

    // Control
    auto widg = ctrl.make_item(ctrl, inlet, doc, nullptr, &context);
    widg->setParentItem(item);
    widg->setPos(0, lab->boundingRect().height());

    // Create a single control
    struct ControlItem {
      score::EmptyRectItem& root;
      Dataflow::PortItem& port;
      score::SimpleTextItem& text;
      decltype(*widg)& control;
    };
    return ControlItem{*item, *port, *lab, *widg};
  }

  template<typename C, typename T>
  static auto makeControlNoText(
      C& ctrl,
      T& inlet,
      QGraphicsItem& parent,
      QObject& context,
      const score::DocumentContext& doc,
      const Process::PortFactoryList& portFactory)
  {
    auto item = new score::EmptyRectItem{&parent};

    // Control
    auto widg = ctrl.make_item(ctrl, inlet, doc, nullptr, &context);
    widg->setParentItem(item);

    // Port
    Process::PortFactory* fact = portFactory.get(inlet.concreteKey());
    auto port = fact->makeItem(inlet, doc, item, &context);

    // Create a single control
    struct ControlItem {
      score::EmptyRectItem& root;
      Dataflow::PortItem& port;
      decltype(*widg)& control;
    };
    return ControlItem{*item, *port, *widg};
  }


  class BackgroundItem : public score::EmptyRectItem
  {
  public:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
    {
      painter->setRenderHint(QPainter::Antialiasing, true);
      painter->setPen(Qt::transparent);
      painter->setBrush(Process::Style::instance().SlotOverlayBorder.getBrush());
      painter->drawRoundedRect(m_rect, 3, 3);
      painter->setRenderHint(QPainter::Antialiasing, false);
    }
  };

  static void item(
      Process::LogFloatSlider& freq,
      Process::FloatSlider& ampl,
      Process::FloatSlider& ampl_fine,
      Process::FloatSlider& offset,
      Process::FloatSlider& offset_fine,
      Process::FloatSlider& jitter,
      Process::FloatSlider& phase,
      Process::Enum& type,
      QGraphicsItem& parent,
      QObject& context,
      const score::DocumentContext& doc)
  {
    const Process::PortFactoryList& portFactory = doc.app.interfaces<Process::PortFactoryList>();
    const auto h = 60;
    const auto w = 70;

    const auto c0 = 10;
    auto freq_item = makeControl(std::get<0>(Metadata::controls), freq, parent, context, doc, portFactory);
    freq_item.root.setPos(c0, 0);

    auto type_item = makeControlNoText(std::get<7>(Metadata::controls), type, parent, context, doc, portFactory);
    type_item.root.setPos(c0, h);
    type_item.control.rows = 2;
    type_item.control.columns = 4;
    type_item.control.setRect(QRectF{0, 0, 150, 40});
    type_item.control.setPos(10, 0);
    type_item.port.setPos(0, 17);

    const auto c1 = 180;
    auto ampl_item = makeControl(std::get<1>(Metadata::controls), ampl, parent, context, doc, portFactory);
    ampl_item.root.setPos(c1, 0);

    auto ampl_fine_item = makeControl(std::get<2>(Metadata::controls), ampl_fine, parent, context, doc, portFactory);
    ampl_fine_item.root.setPos(c1 + w, 0);

    auto offset_item = makeControl(std::get<3>(Metadata::controls), offset, parent, context, doc, portFactory);
    offset_item.root.setPos(c1, h);

    auto offset_fine_item = makeControl(std::get<4>(Metadata::controls), offset_fine, parent, context, doc, portFactory);
    offset_fine_item.root.setPos(c1 + w, h);

    const auto c2 = 250;
    auto jitter_item = makeControl(std::get<5>(Metadata::controls), jitter, parent, context, doc, portFactory);
    jitter_item.root.setPos(c2 + w, 0);

    auto phase_item = makeControl(std::get<6>(Metadata::controls), phase, parent, context, doc, portFactory);
    phase_item.root.setPos(c2 + w, h);
  }

  // With metaclasses, we should instead create structs with named members but where types are either controls, ports, items, inlets...
};
}
}

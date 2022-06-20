#pragma once
#include <Engine/Node/SimpleApi.hpp>
#include <Effect/EffectLayout.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <ossia/network/value/format_value.hpp>
#include <ossia/math/safe_math.hpp>
#include <QPainter>
#include <ossia/detail/math.hpp>
namespace ossia
{

}
namespace Ui
{

namespace SignalDisplay
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Signal display";
    static const constexpr auto objectKey = "SignalDisplay";
    static const constexpr auto category = "Monitoring";
    static const constexpr auto author = "ossia score";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto kind = Process::ProcessCategory::Analyzer;
    static const constexpr auto description = "Visualize an input signal";
    static const constexpr auto flags
        = Process::ProcessFlags::SupportsTemporal;
    static const uuid_constexpr auto uuid
        = make_uuid("9906e563-ddeb-4ecd-908c-952baee2a0a5");

    static const constexpr value_in value_ins[]{"in"};
    static const constexpr auto control_outs
        = tuplet::make_tuple(Control::OutControl{"value"});
  };

  using control_policy = ossia::safe_nodes::precise_tick;

  static void
  run(const ossia::value_port& in,
      ossia::timed_vec<ossia::value>& out_value,
      ossia::token_request tk,
      ossia::exec_state_facade st)
  {
    if (!in.get_data().empty())
    {
      const auto& v = in.get_data().back();
      const float val = ossia::convert<float>(v.value);
      if(ossia::safe_isnan(val) || ossia::safe_isinf(val))
        return;
      out_value[0] = std::vector<ossia::value>{
          double(tk.prev_date.impl) / tk.parent_duration.impl,
          ossia::convert<float>(v.value)};
    }
  }

  struct Layer : public Process::EffectLayerView
  {
  public:
    Scenario::IntervalModel* m_interval{};
    std::vector<std::pair<double, float>> m_values;
    float min = 0;
    float max = 1;

    Layer(
        const Process::ProcessModel& process,
        const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
        , m_interval{Scenario::closestParentInterval(process.parent())}
    {
      if (m_interval)
      {
        connect(
            m_interval,
            &Scenario::IntervalModel::executionEvent,
            this,
            [this](Scenario::IntervalExecutionEvent ev) {
              switch (ev)
              {
                case Scenario::IntervalExecutionEvent::Playing:
                case Scenario::IntervalExecutionEvent::Stopped:
                  reset();
                  break;
                default:
                  break;
              }
            });

        auto out
            = static_cast<Process::ControlOutlet*>(process.outlets().front());

        connect(
            out,
            &Process::ControlOutlet::executionValueChanged,
            this,
            [this](const ossia::value& v) {
              auto& val = *v.target<std::vector<ossia::value>>();
              float float_time = *val[0].target<float>();
              float float_val = *val[1].target<float>();

              m_values.emplace_back(float_time, float_val);

              if (float_val < min)
                min = float_val;
              else if (float_val > max)
                max = float_val;

              update();
            });
      }
    }

    void reset()
    {
      min = 0.;
      max = 1.;
      m_values.clear();
      update();
    }

    void paint_impl(QPainter* p) const override
    {
      if (m_values.size() < 2)
        return;

      p->setRenderHint(QPainter::Antialiasing, true);
      p->setPen(score::Skin::instance().Light.main.pen1_solid_flat_miter);

      const auto w = width();
      const auto h = height();

      const auto to_01 = [&](float v) { return 1. - (v - min) / (max - min); };
      QPointF p0 = {m_values[0].first * w, to_01(m_values[0].second) * h};
      for (std::size_t i = 1; i < m_values.size(); i++)
      {
        const auto& value = m_values[i];
        QPointF p1 = {value.first * w, to_01(value.second) * h};

        if (QLineF l{p0, p1}; l.length() > 2)
        {
          p->drawLine(l);
          p0 = p1;
        }
      }

      p->setRenderHint(QPainter::Antialiasing, false);
    }
  };
};
}


namespace Display
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Value display";
    static const constexpr auto objectKey = "Display";
    static const constexpr auto category = "Monitoring";
    static const constexpr auto author = "ossia score";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto kind = Process::ProcessCategory::Analyzer;
    static const constexpr auto description = "Visualize an input value";
    static const constexpr auto flags = Process::ProcessFlags::TimeIndependent | Process::ProcessFlags::FullyCustomItem;
    static const uuid_constexpr auto uuid
        = make_uuid("3f4a41f2-fa39-420f-ab0f-0af6b8409edb");

    static const constexpr auto controls
        = tuplet::make_tuple(Control::InControl{"value"});
  };

  using control_policy = ossia::safe_nodes::default_tick;

  static void
  run(const ossia::timed_vec<ossia::value>& in,
      ossia::token_request tk,
      ossia::exec_state_facade st)
  {
  }

  struct Layer : public Process::EffectLayerView
  {
  public:
    ossia::value m_value;

    Layer(
        const Process::ProcessModel& process,
        const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
    {
      setAcceptedMouseButtons({});

      const Process::PortFactoryList& portFactory
          = doc.app.interfaces<Process::PortFactoryList>();

      auto inl
          = static_cast<Process::ControlInlet*>(process.inlets().front());

      auto fact = portFactory.get(inl->concreteKey());
      auto port = fact->makePortItem(*inl, doc, this, this);
      port->setPos(0, 5);

      connect(
            inl,
            &Process::ControlInlet::executionValueChanged,
            this,
            [this](const ossia::value& v) {
        m_value = v;
        update();
      });
    }

    void reset()
    {
      m_value = ossia::value{};
      update();
    }

    void paint_impl(QPainter* p) const override
    {
      if (!m_value.valid())
        return;

      p->setRenderHint(QPainter::Antialiasing, true);
      p->setPen(score::Skin::instance().Light.main.pen1_solid_flat_miter);

      p->drawText(boundingRect(), QString::fromStdString(fmt::format("{}", m_value)));

      p->setRenderHint(QPainter::Antialiasing, false);
    }
  };
};
}
}

#pragma once
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <QPainter>

#include <Engine/Node/PdNode.hpp>
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
    static const constexpr auto flags = Process::ProcessFlags::SupportsTemporal;
    static const uuid_constexpr auto uuid = make_uuid("9906e563-ddeb-4ecd-908c-952baee2a0a5");

    static const constexpr value_in value_ins[]{"in"};
    static const constexpr auto control_outs = std::make_tuple(Control::OutControl{"value"});
  };

  using control_policy = ossia::safe_nodes::precise_tick;

  static void
  run(const ossia::value_port& in,
      ossia::safe_nodes::timed_vec<ossia::value>& out_value,
      ossia::token_request tk,
      ossia::exec_state_facade st)
  {
    if (!in.get_data().empty())
    {
      const auto& v = in.get_data().back();
      out_value[0] = std::vector<ossia::value>{
          double(tk.prev_date.impl) / tk.parent_duration.impl, ossia::convert<float>(v.value)};
    }
  }

  struct Layer : public Process::EffectLayerView
  {
  public:
    Scenario::IntervalModel* m_interval{};
    std::vector<std::pair<double, float>> m_values;
    float min = 0;
    float max = 1;

    Layer(const Process::ProcessModel& process, const Process::Context& doc, QGraphicsItem* parent)
        : Process::EffectLayerView{parent}, m_interval{}
    {
      if (auto obj = process.parent())
      {
        do
        {
          m_interval = qobject_cast<Scenario::IntervalModel*>(obj);
          if (m_interval)
            break;
          obj = obj->parent();
        } while (obj);
      }

      if (m_interval)
      {
        connect(m_interval, &Scenario::IntervalModel::executionStarted, this, &Layer::reset);
        connect(m_interval, &Scenario::IntervalModel::executionStopped, this, &Layer::reset);

        auto out = static_cast<Process::ControlOutlet*>(process.outlets().front());

        connect(out, &Process::ControlOutlet::valueChanged, this, [this](const ossia::value& v) {
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
}

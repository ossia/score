#pragma once
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Effect/EffectLayer.hpp>
#include <Effect/EffectLayout.hpp>
// #include <Engine/Node/SimpleApi.hpp>

#include <ossia/detail/math.hpp>
#include <ossia/math/safe_math.hpp>
#include <ossia/network/value/format_value.hpp>

#include <QPainter>

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
namespace Ui
{
using value_out = halp::callback<"out", ossia::value>;
};
namespace Ui::SignalDisplay
{
struct Node
{
  halp_meta(name, "Signal display")
  halp_meta(c_name, "SignalDisplay")
  halp_meta(category, "Monitoring")
  halp_meta(author, "ossia score")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/common-practices/"
      "4-audio.html#analysing-an-audio-signal")
  halp_meta(description, "Visualize an input signal")
  halp_meta(uuid, "9906e563-ddeb-4ecd-908c-952baee2a0a5")

  halp_flag(temporal);
  halp_flag(loops_by_default);

  struct
  {
    halp::val_port<"in", ossia::value> port;
  } inputs;

  struct
  {
    value_out port{};
  } outputs;

  /*
  void operator()()
  {
    if(!in.get_data().empty())
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
*/
  struct Layer : public Process::EffectLayerView
  {
  public:
    Scenario::IntervalModel* m_interval{};
    std::vector<std::pair<double, float>> m_values;
    float min = 0;
    float max = 1;

    Layer(
        const Process::ProcessModel& process, const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
        , m_interval{Scenario::closestParentInterval(process.parent())}
    {
      if(m_interval)
      {
        connect(
            m_interval, &Scenario::IntervalModel::executionEvent, this,
            [this](Scenario::IntervalExecutionEvent ev) {
          switch(ev)
          {
            case Scenario::IntervalExecutionEvent::Playing:
            case Scenario::IntervalExecutionEvent::Stopped:
              reset();
              break;
            default:
              break;
          }
            });

        auto out = static_cast<Process::ControlOutlet*>(process.outlets().front());

        connect(
            out, &Process::ControlOutlet::executionValueChanged, this,
            [this](const ossia::value& v) {
          auto& val = *v.target<std::vector<ossia::value>>();
          float float_time = *val[0].target<float>();
          float float_val = *val[1].target<float>();

          // Handle looping: clear when we jump back in time
          if(!m_values.empty())
            if(float_time < m_values.back().first)
              m_values.clear();

          m_values.emplace_back(float_time, float_val);

          if(float_val < min)
            min = float_val;
          else if(float_val > max)
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
      if(m_values.size() < 2)
        return;

      p->setRenderHint(QPainter::Antialiasing, true);
      p->setPen(score::Skin::instance().Light.main.pen1_solid_flat_miter);

      const auto w = width();
      const auto h = height();

      const auto to_01 = [&](float v) { return 1. - (v - min) / (max - min); };
      QPointF p0 = {m_values[0].first * w, to_01(m_values[0].second) * h};
      for(std::size_t i = 1; i < m_values.size(); i++)
      {
        const auto& value = m_values[i];
        QPointF p1 = {value.first * w, to_01(value.second) * h};

        if(QLineF l{p0, p1}; l.length() > 2)
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

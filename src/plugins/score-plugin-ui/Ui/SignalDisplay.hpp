#pragma once
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Effect/EffectLayer.hpp>
#include <Effect/EffectLayout.hpp>

#include <ossia/detail/math.hpp>
#include <ossia/detail/parse_strict.hpp>
#include <ossia/math/safe_math.hpp>
#include <ossia/network/value/format_value.hpp>

#include <QPainter>

#include <halp/audio.hpp>
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
  halp_flag(fully_custom_item);
  halp_flag(temporal);
  halp_flag(loops_by_default);

  using vec_type = boost::container::small_vector<float, 8>;
  struct
  {
    struct : halp::val_port<"in", std::optional<ossia::value>>
    {
      enum widget
      {
        control
      };
    } port;
  } inputs;

  struct
  {
    struct : halp::val_port<"out", std::optional<vec_type>>
    {
      enum widget
      {
        control
      };
    } port;
  } outputs;

  struct value_visitor
  {
    vec_type& ret;
    void operator()() const noexcept { }
    void operator()(ossia::impulse) const noexcept { ret.push_back(1.f); }
    void operator()(int v) const noexcept { ret.push_back(v); }
    void operator()(float v) const noexcept { ret.push_back(v); }
    void operator()(bool v) const noexcept { ret.push_back(v ? 1.f : 0.f); }
    void operator()(std::string_view v) const noexcept
    {
      if(auto res = ossia::parse_strict<float>(v))
        ret.push_back(*res);
    }
    template <std::size_t N>
    void operator()(std::array<float, N> arr) const noexcept
    {
      ret.insert(ret.end(), arr.begin(), arr.end());
    }
    void operator()(const std::vector<ossia::value>& arr) const noexcept
    {
      ret.reserve(1 + arr.size());
      for(auto& val : arr)
      {
        ret.push_back(ossia::convert<float>(val));
      }
    }
    void operator()(const ossia::value_map_type& arr) const noexcept
    {
      ret.reserve(1 + arr.size());
      for(auto& [k, v] : arr)
      {
        ret.push_back(ossia::convert<float>(v));
      }
    }
  };

  using tick = halp::tick_flicks;
  void operator()(halp::tick_flicks tk)
  {
    if(auto& opt_v = inputs.port.value)
    {
      const auto& v = *opt_v;
      const float val = ossia::convert<float>(v);
      if(ossia::safe_isnan(val) || ossia::safe_isinf(val))
        return;

      vec_type ret;
      ret.push_back(float(tk.relative_position));

      v.apply(value_visitor{ret});

      outputs.port.value = static_cast<vec_type&&>(ret);
    }
  }
  struct Layer : public Process::EffectLayerView
  {
    static constexpr int timestamp_index = 0;
    static constexpr int first_value_index = 1;

  public:
    Scenario::IntervalModel* m_interval{};

    std::vector<vec_type> m_values;
    vec_type min = {0.};
    vec_type max = {1.};

    Layer(
        const Process::ProcessModel& process, const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
        , m_interval{Scenario::closestParentInterval(process.parent())}
    {
      if(m_interval)
      {
        const Process::PortFactoryList& portFactory
            = doc.app.interfaces<Process::PortFactoryList>();

        auto inl = safe_cast<Process::ControlInlet*>(process.inlets().front());

        auto fact = portFactory.get(inl->concreteKey());
        auto port = fact->makePortItem(*inl, doc, this, this);
        port->setPos(0, 5);

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

        auto outl = safe_cast<Process::ControlOutlet*>(process.outlets().front());
        connect(
            outl, &Process::ControlOutlet::valueChanged, this,
            [this](const ossia::value& v) {
          auto& val = *v.target<std::vector<ossia::value>>();
          if(val.size() < 2)
            return;
          int num_values = val.size() - 1;

          vec_type vv;

          for(auto it = val.begin(); it != val.end(); ++it)
          {
            vv.push_back(*it->target<float>());
          }

          // Handle looping: clear when we jump back in time
          if(!m_values.empty() && !m_values.back().empty())
            if(vv[timestamp_index] < m_values.back()[timestamp_index])
            {
              m_values.clear();
            }

          m_values.push_back(std::move(vv));

          // FIXMe
          // if(float_val < min)
          //   min = float_val;
          // else if(float_val > max)
          //   max = float_val;

          update();
        });
      }
    }

    void reset()
    {
      min = {0.};
      max = {1.};
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
      double min = 0, max = 1;

      const auto to_01 = [&](float v) { return 1. - (v - min) / (max - min); };
      QPointF p0 = {
          m_values[0][timestamp_index] * w, to_01(m_values[0][first_value_index]) * h};
      for(std::size_t i = 1; i < m_values.size(); i++)
      {
        const auto& value = m_values[i];
        QPointF p1 = {value[timestamp_index] * w, to_01(value[first_value_index]) * h};

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

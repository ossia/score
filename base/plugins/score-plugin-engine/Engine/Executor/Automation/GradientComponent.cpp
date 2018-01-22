// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "GradientComponent.hpp"
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/score2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <ossia/editor/automation/automation.hpp>
#include <ossia/editor/curve/curve_segment/easing.hpp>
namespace Gradient
{
namespace RecreateOnPlay
{
class gradient_node final : public ossia::graph_node
{
  public:
    using grad_type = boost::container::flat_map<double, ossia::hunter_lab>;

    gradient_node()
    {
      ossia::outlet_ptr vp = ossia::make_outlet<ossia::value_port>();
      vp->data.target<ossia::value_port>()->type = ossia::argb_u{};
      m_outlets.push_back(std::move(vp));
    }

    void set_gradient(grad_type t)
    {
      m_data = std::move(t);
    }

    void handle_before_first(double position)
    {
      auto& out = *m_outlets[0]->data.target<ossia::value_port>();
      auto beg = m_data.begin();
      if(beg->first >= position)
      {
        out.add_raw_value(ossia::argb{beg->second}.dataspace_value);
      }
      else if(!mustTween)
      {
        out.add_raw_value(ossia::argb{beg->second}.dataspace_value);
      }
      else
      {
        if(!tween)
        {
          auto addr = m_outlets[0]->address.target<ossia::net::parameter_base*>();
          if(addr && *addr)
          {
            // TODO if the curve is in another unit, we have to convert it to the correct unit.
            tween = ossia::argb{ossia::convert<ossia::vec4f>((*addr)->value())};
          }
          else
          {
            tween = ossia::argb{beg->second};
          }
        }
        out.add_raw_value(ease_color(0., *tween, beg->first, beg->second, position).dataspace_value);
      }
    }

    void run(ossia::token_request t, ossia::execution_state& e) override
    {
      auto& out = *m_outlets[0]->data.target<ossia::value_port>();

      switch (m_data.size())
      {
        case 0:
          out.add_raw_value(ossia::vec4f{0., 0., 0., 0.});
          return;
        case 1:
          handle_before_first(t.position);
          return;
        default:
        {
          auto it_next = m_data.lower_bound(t.position);
          // Before start
          if (it_next == m_data.begin())
          {
            handle_before_first(t.position);
            return;
          }
          // past end
          if (it_next == m_data.end())
          {
            out.add_raw_value(ossia::argb{m_data.rbegin()->second}.dataspace_value);
            return;
          }

          auto it_prev = it_next;
          --it_prev;

          out.add_raw_value(
                ease_color(it_prev->first, it_prev->second,
                           it_next->first, it_next->second, t.position).dataspace_value);
          return;
        }
      }
    }

    ossia::argb ease_color(double prev_pos, ossia::hunter_lab prev, double next_pos, ossia::hunter_lab next, double pos)
    {
      // Interpolate in La*b* domain
      const auto coeff
          = (pos - prev_pos) / (next_pos - prev_pos);

      ossia::hunter_lab res;
      ossia::easing::ease e{};
      res.dataspace_value = ossia::make_vec(
                              e(prev.dataspace_value[0], next.dataspace_value[0], coeff),
          e(prev.dataspace_value[1], next.dataspace_value[1], coeff),
          e(prev.dataspace_value[2], next.dataspace_value[2], coeff));

      return res;
    }
  public:
    optional<ossia::argb> tween;
  private:
    grad_type m_data;
  public:
    bool mustTween{};
};

class gradient_process final : public ossia::node_process
{
public:
    using ossia::node_process::node_process;
  void start() override
  {
      static_cast<gradient_node*>(node.get())->tween = ossia::none;
  }
};
Component::Component(
    ::Gradient::ProcessModel& element,
    const ::Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : ::Engine::Execution::
      ProcessComponent_T<Gradient::ProcessModel, ossia::node_process>{
        element,
        ctx,
        id, "Executor::GradientComponent", parent}
{
  auto node = std::make_shared<gradient_node>();
  this->node = node;
  m_ossia_process = std::make_shared<gradient_process>(node);


  con(*element.outlet, &Process::Port::addressChanged,
      this, [=] (const auto&) {
    this->in_exec([node] {
      node->tween = ossia::none;
    });
  });

  // TODO the tween case will reset the "running" value,
  // so it may not work perfectly.
  con(element, &Gradient::ProcessModel::tweenChanged,
      this, [=] (bool b) {
    this->in_exec([=] {
      node->tween = ossia::none;
      node->mustTween = b;
    });
  });
  con(element, &Gradient::ProcessModel::gradientChanged,
      this, [this] { this->recompute(); });

  recompute();
}

Component::~Component()
{
}

static ossia::hunter_lab to_ossia_color(const QColor& c)
{
  switch(c.spec())
  {
    case QColor::Rgb:
    {
      ossia::rgb r{(float)c.redF(), (float)c.greenF(), (float)c.blueF()};
      return ossia::hunter_lab{r};
    }
    case QColor::Hsv:
    case QColor::Cmyk:
    case QColor::Hsl:
      return to_ossia_color(c.toRgb());
    case QColor::Invalid:
    default:
      return ossia::hunter_lab{};
  }
}

static auto to_ossia_gradient(const Gradient::ProcessModel::gradient_colors& c)
{
  gradient_node::grad_type g;
  for(auto& e : c)
  {
    g.insert(std::make_pair(e.first, to_ossia_color(e.second)));
  }
  return g;
}

void Component::recompute()
{
  const Engine::Execution::Context& s = this->system();
  auto g = process().gradient();

  s.executionQueue.enqueue(
        [proc=std::dynamic_pointer_cast<gradient_node>(OSSIAProcess().node)
        ,g]
  {
    proc->set_gradient(to_ossia_gradient(g));
  });
}

}
}

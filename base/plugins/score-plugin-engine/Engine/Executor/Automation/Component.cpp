// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/editor/automation/automation.hpp>
#include <ossia/editor/automation/curve_value_visitor.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <QDebug>
#include <QString>
#include <algorithm>
#include <vector>

#include "Component.hpp"
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <ossia/editor/curve/curve.hpp>
#include <ossia/editor/curve/curve_segment.hpp>
#include <ossia/editor/value/value.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/device.hpp>
#include <Engine/CurveConversion.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Engine/score2OSSIA.hpp>
#include <State/Address.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>

#include <ossia/editor/dataspace/dataspace_visitors.hpp> // temporary
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <ossia/dataflow/node_process.hpp>
namespace Automation
{
namespace RecreateOnPlay
{

class automation_node final :
    public ossia::graph_node
{
  public:
    automation_node()
    {
      m_outlets.push_back(ossia::make_outlet<ossia::value_port>());
    }

    ~automation_node() override
    {

    }

    void set_destination(optional<ossia::destination> d)
    {
      if(d)
      {
        m_outlets.front()->address = &d->address();
      }
      else
      {
        m_outlets.front()->address = {};
      }
    }

    void set_behavior(const ossia::behavior& b)
    {
      m_drive = b;
    }

  private:
    void run(ossia::execution_state& e) override
    {
      if(!m_drive)
        return;

      auto& outlet = *m_outlets[0];
      ossia::value_port* vp = outlet.data.target<ossia::value_port>();
      ossia::val_type type = ossia::val_type::FLOAT;
      if(outlet.targets.empty())
      {
        if(auto t = outlet.address.target<ossia::net::parameter_base*>())
        {
          auto param = *t;
          type = param->get_value_type();
        }
      }
      vp->data.push_back(
            ossia::apply(
              ossia::detail::compute_value_visitor{m_position, type}, m_drive));
    }

    ossia::behavior m_drive;
};


Component::Component(
    ::Engine::Execution::IntervalComponent& parentInterval,
    ::Automation::ProcessModel& element,
    const ::Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : ProcessComponent_T{
        parentInterval,
        element,
        ctx,
        id, "Executor::AutomationComponent", parent}
{
  auto node = std::make_shared<automation_node>();
  auto proc = std::make_shared<ossia::node_process>(ctx.plugin.execGraph, node);
  m_ossia_process = proc;
  m_node = node;

  con(element, &Automation::ProcessModel::addressChanged,
      this, [this] (const auto&) { this->recompute(); });
  con(element, &Automation::ProcessModel::minChanged,
      this, [this] (const auto&) { this->recompute(); });
  con(element, &Automation::ProcessModel::maxChanged,
      this, [this] (const auto&) { this->recompute(); });

  // TODO the tween case will reset the "running" value,
  // so it may not work perfectly.
  con(element, &Automation::ProcessModel::tweenChanged,
      this, [this] (const auto&) { this->recompute(); });
  con(element, &Automation::ProcessModel::curveChanged,
      this, [this] () { this->recompute(); });

  ctx.plugin.nodes.insert({element.outlet.get(), m_node});
  ctx.plugin.execGraph->add_node(m_node);
  recompute();
}

Component::~Component()
{
  m_node->clear();
  system().plugin.execGraph->remove_node(m_node);
}

void Component::recompute()
{
  auto dest = Engine::score_to_ossia::makeDestination(
        system().devices.list(),
        process().address());

  if (dest)
  {
    auto& d = *dest;
    auto addressType = d.address().get_value_type();

    auto curve = process().tween()
        ? on_curveChanged(addressType, d)
        : on_curveChanged(addressType, {});

    if (curve)
    {
      system().executionQueue.enqueue(
            [proc=std::dynamic_pointer_cast<automation_node>(m_node)
            ,curve
            ,d_=d]
      {
        proc->set_destination(std::move(d_));
        proc->set_behavior(curve);
      });
      return;
    }
  }
  else
  {
    auto curve = on_curveChanged_impl<float>({});

    if (curve)
    {
      system().executionQueue.enqueue(
            [proc=std::dynamic_pointer_cast<automation_node>(m_node)
            ,curve]
      {
        proc->set_destination({});
        proc->set_behavior(curve);
      });
      return;
    }
  }
}

template <typename Y_T>
std::shared_ptr<ossia::curve_abstract>
Component::on_curveChanged_impl(const optional<ossia::destination>& d)
{
  using namespace ossia;

  const double min = process().min();
  const double max = process().max();

  auto scale_x = [](double val) -> double { return val; };
  auto scale_y = [=](double val) -> Y_T { return val * (max - min) + min; };

  auto segt_data = process().curve().sortedSegments();
  if (segt_data.size() != 0)
  {
    return Engine::score_to_ossia::curve<double, Y_T>(
          scale_x, scale_y, segt_data, d);
  }
  else
  {
    return {};
  }
}

std::shared_ptr<ossia::curve_abstract>
Component::on_curveChanged(
    ossia::val_type type,
    const optional<ossia::destination>& d)
{
  switch (type)
  {
    case ossia::val_type::INT:
      return on_curveChanged_impl<int>(d);
    case ossia::val_type::FLOAT:
      return on_curveChanged_impl<float>(d);
    case ossia::val_type::LIST:
    case ossia::val_type::VEC2F:
    case ossia::val_type::VEC3F:
    case ossia::val_type::VEC4F:
      return on_curveChanged_impl<float>(d);
    default:
      qDebug() << "Unsupported curve type: " << (int)type;
      SCORE_TODO;
  }

  return {};
}
}
}

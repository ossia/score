#include <ossia/editor/automation/automation.hpp>
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
#include <ossia/network/base/address.hpp>
#include <ossia/network/base/node.hpp>
#include <Engine/CurveConversion.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <State/Address.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>

#include <ossia/editor/dataspace/dataspace_visitors.hpp> // temporary
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
namespace Automation
{
namespace RecreateOnPlay
{
Component::Component(
    ::Engine::Execution::ConstraintComponent& parentConstraint,
    ::Automation::ProcessModel& element,
    const ::Engine::Execution::Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
  : ::Engine::Execution::
      ProcessComponent_T<Automation::ProcessModel, ossia::automation>{
        parentConstraint,
        element,
        ctx,
        id, "Executor::AutomationComponent", parent}
{
  m_ossia_process = std::make_shared<ossia::automation>();

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

  recompute();
}

void Component::recompute()
{
  auto dest = Engine::iscore_to_ossia::makeDestination(
        system().devices.list(),
        process().address());

  if (dest)
  {
    auto& d = *dest;
    auto addressType = d.value.get().getValueType();

    auto curve = process().tween()
        ? on_curveChanged(addressType, d)
        : on_curveChanged(addressType, {});

    if (curve)
    {
      system().executionQueue.enqueue(
            [proc=std::dynamic_pointer_cast<ossia::automation>(m_ossia_process)
            ,curve
            ,d_=d]
      {
        proc->setDestination(std::move(d_));
        proc->setBehavior(curve);
      });
      return;
    }
  }

  // If something did not work out
  system().executionQueue.enqueue(
        [proc=std::dynamic_pointer_cast<ossia::automation>(m_ossia_process)]
  {
    proc->clean();
  });
}

template <typename Y_T>
std::shared_ptr<ossia::curve_abstract>
Component::on_curveChanged_impl(const optional<ossia::Destination>& d)
{
  using namespace ossia;

  const double min = process().min();
  const double max = process().max();

  auto scale_x = [](double val) -> double { return val; };
  auto scale_y = [=](double val) -> Y_T { return val * (max - min) + min; };

  auto segt_data = process().curve().sortedSegments();
  if (segt_data.size() != 0)
  {
    return Engine::iscore_to_ossia::curve<double, Y_T>(
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
    const optional<ossia::Destination>& d)
{
  switch (type)
  {
    case ossia::val_type::INT:
      return on_curveChanged_impl<int>(d);
    case ossia::val_type::FLOAT:
      return on_curveChanged_impl<float>(d);
    case ossia::val_type::TUPLE:
    case ossia::val_type::VEC2F:
    case ossia::val_type::VEC3F:
    case ossia::val_type::VEC4F:
      return on_curveChanged_impl<float>(d);
    default:
      qDebug() << "Unsupported curve type: " << (int)type;
      ISCORE_TODO;
  }

  return {};
}
}
}

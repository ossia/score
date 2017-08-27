// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Component.hpp"
#include <ossia/editor/mapper/mapper.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>
#include <ossia/editor/value/value.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/device.hpp>

#include <Engine/Executor/ConstraintComponent.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include <Curve/CurveModel.hpp>

#include <ossia/editor/curve/curve.hpp>
#include <ossia/editor/curve/curve_segment.hpp>
#include <ossia/network/base/parameter.hpp>
#include <Engine/CurveConversion.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>

namespace Mapping
{
namespace RecreateOnPlay
{
Component::Component(
    ::Engine::Execution::ConstraintComponent& parentConstraint,
    ::Mapping::ProcessModel& element,
    const ::Engine::Execution::Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
    : ::Engine::Execution::
          ProcessComponent_T<Mapping::ProcessModel, ossia::mapper>{parentConstraint,
                                                                   element,
                                                                   ctx, id,
                                                                   "MappingElement",
                                                                   parent}
{
  m_ossia_process = std::make_shared<ossia::mapper>();

  con(element, &Mapping::ProcessModel::sourceAddressChanged,
      this, [this] (const auto&) { this->recompute(); });
  con(element, &Mapping::ProcessModel::sourceMinChanged,
      this, [this] (const auto&) { this->recompute(); });
  con(element, &Mapping::ProcessModel::sourceMaxChanged,
      this, [this] (const auto&) { this->recompute(); });

  con(element, &Mapping::ProcessModel::targetAddressChanged,
      this, [this] (const auto&) { this->recompute(); });
  con(element, &Mapping::ProcessModel::targetMinChanged,
      this, [this] (const auto&) { this->recompute(); });
  con(element, &Mapping::ProcessModel::targetMaxChanged,
      this, [this] (const auto&) { this->recompute(); });

  con(element, &Mapping::ProcessModel::curveChanged,
      this, [this] () { this->recompute(); });

  recompute();
}

void Component::recompute()
{
  const auto& devices = system().devices.list();
  auto ossia_source_addr = Engine::iscore_to_ossia::makeDestination(
      devices, process().sourceAddress());

  if (ossia_source_addr)
  {
    auto ossia_target_addr = Engine::iscore_to_ossia::makeDestination(
          devices, process().targetAddress());
    if (ossia_target_addr)
    {
      auto sourceAddressType = ossia_source_addr->address().get_value_type();
      auto targetAddressType = ossia_target_addr->address().get_value_type();

      auto curve = rebuildCurve(sourceAddressType, targetAddressType); // If the type changes we need to rebuild the curve.

      if (curve)
      {
        system().executionQueue.enqueue(
              [proc=std::dynamic_pointer_cast<ossia::mapper>(m_ossia_process)
              ,curve
              ,source=*ossia_source_addr
              ,target=*ossia_target_addr]
        {
          proc->set_driver(std::move(source));
          proc->set_driven(std::move(target));
          proc->set_behavior(std::move(curve));
        });
        return;
      }
    }
  }

  // If something did not work out
  system().executionQueue.enqueue(
        [proc=std::dynamic_pointer_cast<ossia::mapper>(m_ossia_process)]
  {
     proc->clean();
  });

}

template <typename X_T, typename Y_T>
std::shared_ptr<ossia::curve_abstract> Component::on_curveChanged_impl2()
{
  if (process().curve().segments().size() == 0)
    return {};

  const double xmin = process().sourceMin();
  const double xmax = process().sourceMax();

  const double ymin = process().targetMin();
  const double ymax = process().targetMax();

  auto scale_x = [=](double val) -> X_T { return val * (xmax - xmin) + xmin; };
  auto scale_y = [=](double val) -> Y_T { return val * (ymax - ymin) + ymin; };

  auto segt_data = process().curve().sortedSegments();
  if (segt_data.size() != 0)
  {
    return Engine::iscore_to_ossia::curve<X_T, Y_T>(
        scale_x, scale_y, segt_data, {});
  }
  else
  {
    return {};
  }
}

template <typename X_T>
std::shared_ptr<ossia::curve_abstract> Component::on_curveChanged_impl(
    ossia::val_type target)
{
  switch (target)
  {
    case ossia::val_type::INT:
      return on_curveChanged_impl2<X_T, int>();
      break;
    case ossia::val_type::FLOAT:
    case ossia::val_type::LIST:
    case ossia::val_type::VEC2F:
    case ossia::val_type::VEC3F:
    case ossia::val_type::VEC4F:
      return on_curveChanged_impl2<X_T, float>();
      break;
    default:
      qDebug() << "Unsupported target address type: "
               << (int)target;
      ISCORE_TODO;
  }

  return {};
}

std::shared_ptr<ossia::curve_abstract> Component::rebuildCurve(
    ossia::val_type source,
    ossia::val_type target)
{
  switch (source)
  {
    case ossia::val_type::INT:
      return on_curveChanged_impl<int>(target);
    case ossia::val_type::FLOAT:
    case ossia::val_type::LIST:
    case ossia::val_type::VEC2F:
    case ossia::val_type::VEC3F:
    case ossia::val_type::VEC4F:
      return on_curveChanged_impl<float>(target);
    default:
      qDebug() << "Unsupported source address type: "
               << (int)source;
      ISCORE_TODO;
  }

  return {};
}
}
}

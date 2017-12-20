// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Component.hpp"
#include <ossia/editor/mapper/mapper.hpp>
#include <ossia/editor/mapper/detail/mapper_visitor.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>
#include <ossia/network/value/value.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/device.hpp>

#include <Engine/Executor/IntervalComponent.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <score/document/DocumentInterface.hpp>

#include <Curve/CurveModel.hpp>

#include <ossia/editor/curve/curve.hpp>
#include <ossia/editor/curve/curve_segment.hpp>
#include <ossia/network/base/parameter.hpp>
#include <Engine/CurveConversion.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/score2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/misc_visitors.hpp>

namespace Mapping
{
namespace RecreateOnPlay
{
class mapping_node final :
    public ossia::graph_node
{
  public:
    mapping_node()
    {
      m_inlets.push_back(ossia::make_inlet<ossia::value_port>());
      m_outlets.push_back(ossia::make_outlet<ossia::value_port>());
    }

    ~mapping_node() override
    {

    }

    void set_driver(optional<ossia::destination> d)
    {
      if(d)
      {
        m_inlets.front()->address = &d->address();
      }
      else
      {
        m_inlets.front()->address = {};
      }
    }

    void set_driven(optional<ossia::destination> d)
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
    void run(ossia::token_request t, ossia::execution_state& e) override
    {
      if(!m_drive)
        return;

      auto& inlet = *m_inlets[0];
      auto& outlet = *m_outlets[0];
      ossia::value_port* ip = inlet.data.target<ossia::value_port>();
      ossia::value_port* op = outlet.data.target<ossia::value_port>();

      // TODO use correct unit / whatever ?
      for(auto& tv : ip->get_data())
      {
        if(tv.value.valid())
        {
          ossia::tvalue newval = tv;
          newval.value = ossia::apply(ossia::detail::mapper_compute_visitor{}, tv.value, m_drive.v);

          op->add_raw_value(std::move(newval));
        }
      }
    }

    ossia::behavior m_drive;
};
Component::Component(
    ::Mapping::ProcessModel& element,
    const ::Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ::Engine::Execution::
          ProcessComponent_T<Mapping::ProcessModel, ossia::node_process>{
                                                                   element,
                                                                   ctx, id,
                                                                   "MappingElement",
                                                                   parent}
{
  auto node = std::make_shared<mapping_node>();
  m_ossia_process = std::make_shared<ossia::node_process>(node);

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

  ctx.plugin.register_node(process(), node);
  recompute();
}

Component::~Component()
{
}

void Component::recompute()
{
  const auto& devices = system().devices.list();
  auto ossia_source_addr = Engine::score_to_ossia::makeDestination(
      devices, process().sourceAddress());
  auto ossia_target_addr = Engine::score_to_ossia::makeDestination(
        devices, process().targetAddress());

  std::shared_ptr<ossia::curve_abstract> curve;
  if (ossia_source_addr && ossia_target_addr)
  {
    auto sourceAddressType = ossia_source_addr->address().get_value_type();
    auto targetAddressType = ossia_target_addr->address().get_value_type();

    curve = rebuildCurve(sourceAddressType, targetAddressType); // If the type changes we need to rebuild the curve.
  }
  else
  {
    curve = rebuildCurve(ossia::val_type::FLOAT, ossia::val_type::FLOAT);
  }

  if (curve)
  {
    in_exec(
          [proc=std::dynamic_pointer_cast<mapping_node>(OSSIAProcess().node)
          ,curve
          ,ossia_source_addr
          ,ossia_target_addr]
    {
      proc->set_driver(std::move(ossia_source_addr));
      proc->set_driven(std::move(ossia_target_addr));
      proc->set_behavior(std::move(curve));
    });
    return;
  }
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
    return Engine::score_to_ossia::curve<X_T, Y_T>(
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
      SCORE_TODO;
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
      SCORE_TODO;
  }

  return {};
}
}
}

#include "Component.hpp"
#include <ossia/editor/mapper/mapper.hpp>
#include <ossia/editor/state/state.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/value/value.hpp>
#include <ossia/network/base/node.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Engine/Executor/ConstraintElement.hpp>


#include <Curve/CurveModel.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Engine/CurveConversion.hpp>
#include <ossia/editor/curve/curve.hpp>
#include <ossia/editor/curve/curve_segment.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <ossia/network/base/address.hpp>

namespace Mapping
{
namespace RecreateOnPlay
{
Component::Component(
        ::Engine::Execution::ConstraintElement& parentConstraint,
        ::Mapping::ProcessModel& element,
        const ::Engine::Execution::Context& ctx,
        const Id<iscore::Component>& id,
        QObject *parent):
    ::Engine::Execution::ProcessComponent_T<Mapping::ProcessModel, ossia::mapper>{parentConstraint, element, ctx, id, "MappingElement", parent},
    m_deviceList{ctx.devices.list()}
{
    recreate();
}

template<typename X_T, typename Y_T>
std::shared_ptr<ossia::curve_abstract> Component::on_curveChanged_impl2()
{
    if(process().curve().segments().size() == 0)
        return {};

    const double xmin = process().sourceMin();
    const double xmax = process().sourceMax();

    const double ymin = process().targetMin();
    const double ymax = process().targetMax();

    auto scale_x = [=] (double val) -> X_T { return val * (xmax - xmin) + xmin; };
    auto scale_y = [=] (double val) -> Y_T { return val * (ymax - ymin) + ymin; };

    auto segt_data = process().curve().sortedSegments();
    if(segt_data.size() != 0)
    {
        return Engine::iscore_to_ossia::curve<X_T, Y_T>(scale_x, scale_y, segt_data, {});
    }
    else
    {
        return {};
    }
}

template<typename X_T>
std::shared_ptr<ossia::curve_abstract> Component::on_curveChanged_impl()
{
    switch(m_targetAddressType)
    {
        case ossia::val_type::INT:
            return on_curveChanged_impl2<X_T, int>();
            break;
        case ossia::val_type::FLOAT:
            return on_curveChanged_impl2<X_T, float>();
            break;
        default:
            qDebug() << "Unsupported target address type: " << (int)m_targetAddressType;
            ISCORE_TODO;
    }

    return {};
}

std::shared_ptr<ossia::curve_abstract> Component::rebuildCurve()
{
    m_ossia_curve.reset();
    switch(m_sourceAddressType)
    {
        case ossia::val_type::INT:
            m_ossia_curve = on_curveChanged_impl<int>();
            break;
        case ossia::val_type::FLOAT:
            m_ossia_curve = on_curveChanged_impl<float>();
            break;
        default:
            qDebug() << "Unsupported source address type: " << (int)m_sourceAddressType;
            ISCORE_TODO;
    }

    return m_ossia_curve;
}

void Component::recreate()
{
    m_ossia_curve.reset(); // It will be remade after.
    m_ossia_process = nullptr;

    auto ossia_source_addr = Engine::iscore_to_ossia::findAddress(
                m_deviceList,
                this->system().doc,
                process().sourceAddress());
    if(!ossia_source_addr)
        return;

    auto ossia_target_addr = Engine::iscore_to_ossia::findAddress(
                m_deviceList,
                this->system().doc,
                process().targetAddress());
    if(!ossia_target_addr)
        return;

    m_sourceAddressType = ossia_source_addr->getValueType();
    m_targetAddressType = ossia_target_addr->getValueType();

    rebuildCurve(); // If the type changes we need to rebuild the curve.

    if(m_ossia_curve)
    {
        m_ossia_process = new ossia::mapper(
                    *ossia_source_addr,
                    *ossia_target_addr,
                    ossia::Behavior(m_ossia_curve));
    }
}

}
}

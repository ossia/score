#include "Component.hpp"

#include <Editor/Mapper.h>
#include <Editor/State.h>
#include <Editor/Message.h>
#include <Editor/Value/Value.h>
#include <Editor/Value/Behavior.h>
#include <OSSIA/iscore2OSSIA.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <OSSIA/Protocols/OSSIADevice.hpp>
#include <OSSIA/Executor/ConstraintElement.hpp>


#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <OSSIA/CurveConversion.hpp>
#include <OSSIA/Protocols/OSSIADevice.hpp>
#include <Editor/Curve.h>
#include <Editor/CurveSegment/CurveSegmentLinear.h>
#include <Editor/CurveSegment/CurveSegmentPower.h>
#include <OSSIA/Executor/ExecutorContext.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>

namespace Mapping
{
namespace RecreateOnPlay
{
Component::Component(
        ::RecreateOnPlay::ConstraintElement& parentConstraint,
        ::Mapping::ProcessModel& element,
        const ::RecreateOnPlay::Context& ctx,
        const Id<iscore::Component>& id,
        QObject *parent):
    ::RecreateOnPlay::ProcessComponent_T<Mapping::ProcessModel>{parentConstraint, element, ctx, id, "MappingElement", parent},
    m_deviceList{ctx.devices.list()}
{
    recreate();
}

template<typename X_T, typename Y_T>
std::shared_ptr<OSSIA::CurveAbstract> Component::on_curveChanged_impl2()
{
    if(process().curve().segments().size() == 0)
        return {};

    const double xmin = process().sourceMin();
    const double xmax = process().sourceMax();

    const double ymin = process().targetMin();
    const double ymax = process().targetMax();

    auto scale_x = [=] (double val) -> X_T { return val * (xmax - xmin) + xmin; };
    auto scale_y = [=] (double val) -> Y_T { return val * (ymax - ymin) + ymin; };

    auto segt_data = process().curve().toCurveData();

    if(segt_data.size() != 0)
    {
        std::sort(segt_data.begin(), segt_data.end());
        return iscore::convert::curve<X_T, Y_T>(scale_x, scale_y, segt_data);
    }
    else
    {
        return {};
    }
}

template<typename X_T>
std::shared_ptr<OSSIA::CurveAbstract> Component::on_curveChanged_impl()
{
    switch(m_targetAddressType)
    {
        case OSSIA::Type::INT:
            return on_curveChanged_impl2<X_T, int>();
            break;
        case OSSIA::Type::FLOAT:
            return on_curveChanged_impl2<X_T, float>();
            break;
        default:
            qDebug() << "Unsupported target address type: " << (int)m_targetAddressType;
            ISCORE_TODO;
    }

    return {};
}

std::shared_ptr<OSSIA::CurveAbstract> Component::rebuildCurve()
{
    m_ossia_curve.reset();
    switch(m_sourceAddressType)
    {
        case OSSIA::Type::INT:
            m_ossia_curve = on_curveChanged_impl<int>();
            break;
        case OSSIA::Type::FLOAT:
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
    auto iscore_source_addr = process().sourceAddress();
    auto iscore_target_addr = process().targetAddress();

    std::shared_ptr<OSSIA::Address> ossia_source_addr;
    std::shared_ptr<OSSIA::Address> ossia_target_addr;

    m_ossia_curve.reset(); // It will be remade after.

    // Get the device list to obtain the nodes.
    const auto& devices = m_deviceList.devices();

    // TODO use this in automation
    auto getAddress = [&] (const State::Address& addr) -> std::shared_ptr<OSSIA::Address>
    {
        // Look for the real node in the device
        auto dev_it = std::find_if(devices.begin(), devices.end(),
                                   [&] (Device::DeviceInterface* dev) {
            return dev->settings().name == addr.device;
        });

        if(dev_it == devices.end())
            return {};

        auto dev = dynamic_cast<Ossia::OSSIADevice*>(*dev_it);
        if(!dev)
            return {};

        auto node = iscore::convert::findNodeFromPath(addr.path, &dev->impl());
        if(!node)
            return {};

        // Add the real address
        auto address = node->getAddress();
        if(!address)
            return {};
        return address;
    };

    ossia_source_addr = getAddress(iscore_source_addr);
    if(!ossia_source_addr)
        goto curve_cleanup_label;

    m_sourceAddressType = ossia_source_addr->getValueType();

    ossia_target_addr = getAddress(iscore_target_addr);
    if(!ossia_target_addr)
        goto curve_cleanup_label;

    m_targetAddressType = ossia_target_addr->getValueType();


    using namespace OSSIA;
    rebuildCurve(); // If the type changes we need to rebuild the curve.
    if(!m_ossia_curve)
        goto curve_cleanup_label;

    // TODO on_min/max changed
    m_ossia_process = OSSIA::Mapper::create(
                ossia_source_addr, ossia_target_addr,
                Behavior(m_ossia_curve));

    return;

curve_cleanup_label:
    m_ossia_process.reset();
    return;
}

}
}

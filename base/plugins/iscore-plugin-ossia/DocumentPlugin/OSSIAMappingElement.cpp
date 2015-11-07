#include "OSSIAMappingElement.hpp"

#include <API/Headers/Editor/Mapper.h>
#include <API/Headers/Editor/State.h>
#include <API/Headers/Editor/Message.h>
#include <API/Headers/Editor/Value.h>
#include <Mapping/MappingModel.hpp>
#include "iscore2OSSIA.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include "Protocols/OSSIADevice.hpp"
#include "OSSIAConstraintElement.hpp"

#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Linear/LinearCurveSegmentModel.hpp>
#include <Curve/Segment/Power/PowerCurveSegmentModel.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include "iscore2OSSIA.hpp"
#include "Protocols/OSSIADevice.hpp"
#include <API/Headers/Editor/Curve.h>
#include <API/Headers/Editor/CurveSegment/CurveSegmentLinear.h>
#include <API/Headers/Editor/CurveSegment/CurveSegmentPower.h>

OSSIAMappingElement::OSSIAMappingElement(
        OSSIAConstraintElement& parentConstraint,
        MappingModel& element,
        QObject *parent):
    OSSIAProcessElement{parentConstraint, parent},
    m_iscore_mapping{element},
    m_deviceList{iscore::IDocument::documentFromObject(element)->model().pluginModel<DeviceDocumentPlugin>()->list()}
{
    using namespace iscore::convert;

    con(element, &MappingModel::curveChanged,
        this, &OSSIAMappingElement::rebuild); // We have to recreate the Mapping in all cases

    rebuild();
}

std::shared_ptr<OSSIA::TimeProcess> OSSIAMappingElement::OSSIAProcess() const
{
    return {}; //m_ossia_mapping;
}

Process& OSSIAMappingElement::iscoreProcess() const
{
    return m_iscore_mapping;
}

template<typename X_T, typename Y_T>
std::shared_ptr<OSSIA::CurveAbstract> OSSIAMappingElement::on_curveChanged_impl2()
{
    using namespace OSSIA;
    auto curve = Curve<X_T, Y_T>::create();

    const double xmin = m_iscore_mapping.sourceMin();
    const double xmax = m_iscore_mapping.sourceMax();

    const double ymin = m_iscore_mapping.targetMin();
    const double ymax = m_iscore_mapping.targetMax();

    auto scale_x = [=] (double val) -> X_T { return val * (xmax - xmin) + xmin; };
    auto scale_y = [=] (double val) -> Y_T { return val * (ymax - ymin) + ymin; };

    // For now we will assume that every segment is dynamic
    for(const auto& iscore_segment : m_iscore_mapping.curve().segments())
    {
        if(auto segt = dynamic_cast<const LinearCurveSegmentModel*>(&iscore_segment))
        {
            auto linearSegment = CurveSegmentLinear<Y_T>::create(curve);
            curve->addPoint(linearSegment, scale_x(segt->end().x()), scale_y(segt->end().y()));
        }
        else if(auto segt = dynamic_cast<const PowerCurveSegmentModel*>(&iscore_segment))
        {
            if(segt->gamma == 12.05)
            {
                auto linearSegment = CurveSegmentLinear<Y_T>::create(curve);
                curve->addPoint(linearSegment, scale_x(segt->end().x()), scale_y(segt->end().y()));
            }
            else
            {
                auto powSegment = CurveSegmentPower<Y_T>::create(curve);
                powSegment->setPower(12.05 - segt->gamma); // TODO document this somewhere.
                curve->addPoint(powSegment, scale_x(segt->end().x()), scale_y(segt->end().y()));
            }
        }

        if(iscore_segment.start().x() == 0.)
        {
            curve->setInitialValue(scale_y(iscore_segment.start().y()));
        }
    }

    m_ossia_curve = curve;
    return m_ossia_curve;
}

template<typename X_T>
std::shared_ptr<OSSIA::CurveAbstract> OSSIAMappingElement::on_curveChanged_impl()
{
    switch(m_targetAddressType)
    {
        case OSSIA::Value::Type::INT:
            on_curveChanged_impl2<X_T, int>();
            break;
        case OSSIA::Value::Type::FLOAT:
            on_curveChanged_impl2<X_T, float>();
            break;
        default:
            m_ossia_curve.reset();
            qDebug() << "Unsupported target address type: " << (int)m_targetAddressType;
            ISCORE_TODO;
    }

    return m_ossia_curve;
}

std::shared_ptr<OSSIA::CurveAbstract> OSSIAMappingElement::rebuildCurve()
{
    switch(m_sourceAddressType)
    {
        case OSSIA::Value::Type::INT:
            on_curveChanged_impl<int>();
            break;
        case OSSIA::Value::Type::FLOAT:
            on_curveChanged_impl<float>();
            break;
        default:
            m_ossia_curve.reset();
            qDebug() << "Unsupported source address type: " << (int)m_sourceAddressType;
            ISCORE_TODO;
    }

    return m_ossia_curve;
}

void OSSIAMappingElement::rebuild()
{
    auto iscore_source_addr = m_iscore_mapping.sourceAddress();
    auto iscore_target_addr = m_iscore_mapping.targetAddress();

    std::shared_ptr<OSSIA::Address> ossia_source_addr;
    std::shared_ptr<OSSIA::Address> ossia_target_addr;
    std::shared_ptr<OSSIA::Mapper> new_mapping;

    // The updating routine
    auto update = [&] (auto new_mapping) {
        auto old_mapping = m_ossia_mapping;
        m_ossia_mapping = new_mapping;
        emit changed(old_mapping, new_mapping);
    };


    m_ossia_curve.reset(); // It will be remade after.

    // Get the device list to obtain the nodes.
    auto doc = iscore::IDocument::documentFromObject(m_iscore_mapping);
    auto plug = doc->model().pluginModel<DeviceDocumentPlugin>();
    const auto& devices = plug->list().devices();

    // TODO use this in automation
    auto getAddress = [&] (const iscore::Address& addr) -> std::shared_ptr<OSSIA::Address>
    {
        // Look for the real node in the device
        auto dev_it = std::find_if(devices.begin(), devices.end(),
                                   [&] (DeviceInterface* dev) {
            return dev->settings().name == addr.device;
        });

        if(dev_it == devices.end())
            return {};

        auto dev = dynamic_cast<OSSIADevice*>(*dev_it);
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
    new_mapping = OSSIA::Mapper::create(
                ossia_source_addr, ossia_target_addr,
                new Behavior(m_ossia_curve));

    update(new_mapping);

    return;

curve_cleanup_label:
    update(std::shared_ptr<OSSIA::Mapper>{}); // Cleanup
    return;
}


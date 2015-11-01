#include "OSSIAAutomationElement.hpp"

#include <API/Headers/Editor/Automation.h>
#include <API/Headers/Editor/State.h>
#include <API/Headers/Editor/Message.h>
#include <API/Headers/Editor/Value.h>
#include <Automation/AutomationModel.hpp>
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

OSSIAAutomationElement::OSSIAAutomationElement(
        OSSIAConstraintElement& parentConstraint,
        AutomationModel& element,
        QObject *parent):
    OSSIAProcessElement{parentConstraint, parent},
    m_iscore_autom{element},
    m_deviceList{iscore::IDocument::documentFromObject(element)->model().pluginModel<DeviceDocumentPlugin>()->list()}
{
    using namespace iscore::convert;

    con(element, &AutomationModel::curveChanged,
        this, [&] () {
        rebuild();
    }); // We have to recreate the automation in all cases

    rebuild();
}

std::shared_ptr<OSSIA::TimeProcess> OSSIAAutomationElement::OSSIAProcess() const
{
    return m_ossia_autom;
}

Process& OSSIAAutomationElement::iscoreProcess() const
{
    return m_iscore_autom;
}

void OSSIAAutomationElement::rebuild()
{
    auto addr = m_iscore_autom.address();
    std::shared_ptr<OSSIA::Automation> new_autom;
    std::shared_ptr<OSSIA::Address> address;
    OSSIA::Node* node{};
    OSSIADevice* dev{};
    // The updating routine
    auto update = [&] (auto new_autom) {
        auto old_autom = m_ossia_autom;
        m_ossia_autom = new_autom;
        emit changed(old_autom, new_autom);
    };

    m_ossia_curve.reset(); // It will be remade after.

    // Get the device list to obtain the nodes.
    auto doc = iscore::IDocument::documentFromObject(m_iscore_autom);
    auto plug = doc->model().pluginModel<DeviceDocumentPlugin>();
    const auto& devices = plug->list().devices();

    // Look for the real node in the device
    auto dev_it = std::find_if(devices.begin(), devices.end(),
                               [&] (DeviceInterface* dev) {
        return dev->settings().name == addr.device;
    });

    if(dev_it == devices.end())
        goto curve_cleanup_label;

    dev = dynamic_cast<OSSIADevice*>(*dev_it);
    if(!dev)
        goto curve_cleanup_label;

    node = iscore::convert::findNodeFromPath(addr.path, &dev->impl());
    if(!node)
        goto curve_cleanup_label;

    // Add the real address
    address = node->getAddress();
    if(!address)
        goto curve_cleanup_label;

    m_addressType = address->getValueType();


    using namespace OSSIA;
    on_curveChanged(); // If the type changes we need to rebuild the curve.
    if(!m_ossia_curve)
        goto curve_cleanup_label;

    // TODO on_min/max changed
    new_autom = Automation::create(
                address,
                new Behavior(m_ossia_curve));

    update(new_autom);

    return;

curve_cleanup_label:
    update(std::shared_ptr<OSSIA::Automation>{}); // Cleanup
    return;
}

template<typename Y_T>
std::shared_ptr<OSSIA::CurveAbstract> OSSIAAutomationElement::on_curveChanged_impl()
{
    using namespace OSSIA;
    using X_T = float;
    auto curve = Curve<X_T, Y_T>::create();

    const double min = m_iscore_autom.min();
    const double max = m_iscore_autom.max();

    auto scale = [=] (double val) -> Y_T { return val * (max - min) + min; };

    // For now we will assume that every segment is dynamic
    for(const auto& iscore_segment : m_iscore_autom.curve().segments())
    {
        if(auto segt = dynamic_cast<const LinearCurveSegmentModel*>(&iscore_segment))
        {
            auto linearSegment = CurveSegmentLinear<Y_T>::create(curve);
            curve->addPoint(linearSegment, X_T(segt->end().x()), scale(segt->end().y()));
        }
        else if(auto segt = dynamic_cast<const PowerCurveSegmentModel*>(&iscore_segment))
        {
            if(segt->gamma == 12.05)
            {
                auto linearSegment = CurveSegmentLinear<Y_T>::create(curve);
                curve->addPoint(linearSegment, X_T(segt->end().x()), scale(segt->end().y()));
            }
            else
            {
                auto powSegment = CurveSegmentPower<Y_T>::create(curve);
                powSegment->setPower(12.05 - segt->gamma); // TODO document this somewhere.
                curve->addPoint(powSegment, X_T(segt->end().x()), scale(segt->end().y()));
            }
        }

        if(iscore_segment.start().x() == 0.)
        {
            curve->setInitialValue(scale(iscore_segment.start().y()));
        }
    }

    m_ossia_curve = curve;
    return m_ossia_curve;
}

std::shared_ptr<OSSIA::CurveAbstract> OSSIAAutomationElement::on_curveChanged()
{
    switch(m_addressType)
    {
        case OSSIA::Value::Type::INT:
            on_curveChanged_impl<int>();
            break;
        case OSSIA::Value::Type::FLOAT:
            on_curveChanged_impl<float>();
            break;
        default:
            m_ossia_curve.reset();
            qDebug() << "Unsupported curve type: " << (int)m_addressType;
            ISCORE_TODO;
    }

    return m_ossia_curve;
}

#include "OSSIAAutomationElement.hpp"

#include <API/Headers/Editor/Automation.h>
#include <API/Headers/Editor/State.h>
#include <API/Headers/Editor/Message.h>
#include <API/Headers/Editor/Value.h>
#include "../iscore-plugin-curve/Automation/AutomationModel.hpp"
#include "iscore2OSSIA.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include "../iscore-plugin-deviceexplorer/Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp"
#include "Protocols/OSSIADevice.hpp"
#include "OSSIAConstraintElement.hpp"

#include "../iscore-plugin-curve/Curve/CurveModel.hpp"
#include "../iscore-plugin-curve/Curve/Segment/LinearCurveSegmentModel.hpp"

#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include "iscore2OSSIA.hpp"
#include "Protocols/OSSIADevice.hpp"
#include <API/Headers/Editor/Curve.h>
#include <API/Headers/Editor/CurveSegment/CurveSegmentLinear.h>

OSSIAAutomationElement::OSSIAAutomationElement(
        OSSIAConstraintElement* parentConstraint,
        const AutomationModel *element,
        QObject *parent):
    OSSIAProcessElement{parent},
    m_parent_constraint{parentConstraint},
    m_iscore_autom{element},
    m_deviceList{static_cast<DeviceDocumentPlugin>(iscore::IDocument::documentFromObject(element)->model()->pluginModel("DeviceDocumentPlugin")).list()}
{
    using namespace iscore::convert;

    connect(element, &AutomationModel::addressChanged,
            this, &OSSIAAutomationElement::on_addressChanged);
    connect(element, &AutomationModel::curveChanged,
            this, [&] () {
        on_addressChanged(m_iscore_autom->address());
    }); // We have to recreate the automation in all cases

    on_addressChanged(element->address());
}

std::shared_ptr<OSSIA::TimeProcess> OSSIAAutomationElement::process() const
{
    return m_ossia_autom;
}

const Process *OSSIAAutomationElement::iscoreProcess() const
{
    return m_iscore_autom;
}

void OSSIAAutomationElement::on_addressChanged(const iscore::Address& addr)
{
    // The updating routine
    auto update = [&] (auto new_autom) {
        auto old_autom = m_ossia_autom;
        m_ossia_autom = new_autom;
        emit changed(old_autom, new_autom);
    };

    m_ossia_curve.reset(); // It will be remade after.

    // Get the device list to obtain the nodes.
    auto doc = iscore::IDocument::documentFromObject(m_iscore_autom);
    auto plug = static_cast<DeviceDocumentPlugin*>(doc->model()->pluginModel("DeviceDocumentPlugin"));
    const auto& devices = plug->list().devices();

    // Look for the real node in the device
    auto dev_it = std::find_if(devices.begin(), devices.end(),
                               [&] (DeviceInterface* dev) {
        return dev->settings().name == addr.device;
    });

    if(dev_it == devices.end())
    {
        update(std::shared_ptr<OSSIA::Automation>{}); // Cleanup
        return;
    }

    auto node = iscore::convert::findNodeFromPath(addr.path, &static_cast<OSSIADevice&>(**dev_it).impl());
    if(!node)
    {
        update(std::shared_ptr<OSSIA::Automation>{}); // Cleanup
        return;
    }

    // Add the real address
    auto address = node->getAddress();
    if(!address)
    {
        update(std::shared_ptr<OSSIA::Automation>{}); // Cleanup
        return;
    }
    m_addressType = address->getValueType();


    using namespace OSSIA;
    on_curveChanged(); // If the type changes we need to rebuild the curve.
    // TODO on_min/max changed
    auto new_autom = Automation::create(
                address,
                new Behavior(m_ossia_curve));


    // TODO Fetch from the State in i-score
    // TODO continue for other types
    if(m_addressType == OSSIA::Value::Type::FLOAT)
    {
        auto first_start_message = Message::create(address, new Float(0.));
        new_autom->getStartState()->stateElements().push_back(first_start_message);

        auto first_end_message = Message::create(address, new Float(1.));
        new_autom->getEndState()->stateElements().push_back(first_end_message);
    }
    else if(m_addressType == OSSIA::Value::Type::INT)
    {
        auto first_start_message = Message::create(address, new Int(0.));
        new_autom->getStartState()->stateElements().push_back(first_start_message);

        auto first_end_message = Message::create(address, new Int(1.));
        new_autom->getEndState()->stateElements().push_back(first_end_message);
    }
    else
    {
        ISCORE_TODO;
    }

    update(new_autom);
}

template<typename T>
void OSSIAAutomationElement::on_curveChanged_impl()
{
    using namespace OSSIA;
    auto curve = Curve<T>::create();

    const double min = m_iscore_autom->min();
    const double max = m_iscore_autom->max();

    auto scale = [=] (double val) -> T { return val * (max - min) + min; };

    // For now we will assume that every segment is dynamic
    for(const auto& iscore_segment : m_iscore_autom->curve().segments())
    {
        auto linearSegment = CurveSegmentLinear<T>::create(curve);
        curve->addPoint(iscore_segment.start().x(), scale(iscore_segment.start().y()), linearSegment);
        curve->addPoint(iscore_segment.end().x(), scale(iscore_segment.end().y()), linearSegment);
    }

    //curve->setInitialValue((*m_iscore_autom->curve().segments().begin()).start().y());
    m_ossia_curve = curve;
}

void OSSIAAutomationElement::on_curveChanged()
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
            qDebug() << "Unsupported curve type: " << (int)m_addressType;
            ISCORE_TODO;
            return;
    }
}

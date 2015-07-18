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
            this, &OSSIAAutomationElement::on_curveChanged);
    on_curveChanged(); // Rebuild the curve with the correct data.
    on_addressChanged(element->address());
}

std::shared_ptr<OSSIA::TimeProcess> OSSIAAutomationElement::process() const
{
    return m_ossia_autom;
}

const ProcessModel *OSSIAAutomationElement::iscoreProcess() const
{
    return m_iscore_autom;
}

#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include "iscore2OSSIA.hpp"
#include "Protocols/OSSIADevice.hpp"
#include <API/Headers/Editor/Curve.h>
#include <API/Headers/Editor/CurveSegment/CurveSegmentLinear.h>
void OSSIAAutomationElement::on_addressChanged(const iscore::Address& addr)
{
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
        // TODO clear the automation
        return;
    }

    auto node = iscore::convert::findNodeFromPath(addr.path, &static_cast<OSSIADevice&>(**dev_it).impl());
    if(!node)
    {
        // TODO clear the automation
        return;
    }

    // Add the real address
    auto address = node->getAddress();


    using namespace OSSIA;

    auto new_autom = Automation::create(
                [](const OSSIA::TimeValue& position,
                   const OSSIA::TimeValue& date,
                   std::shared_ptr<OSSIA::State> state) { state->launch(); },
                address,
                new Behavior(m_ossia_curve));

    // Fetch from the State in i-score
    auto first_start_message = Message::create(address, new Float(0.));
    new_autom->getStartState()->stateElements().push_back(first_start_message);

    auto first_end_message = Message::create(address, new Float(1.));
    new_autom->getEndState()->stateElements().push_back(first_end_message);

    auto old_autom = m_ossia_autom;
    m_ossia_autom = new_autom;
    emit changed(old_autom, new_autom);
}

#include "../iscore-plugin-curve/Curve/CurveModel.hpp"
#include "../iscore-plugin-curve/Curve/Segment/LinearCurveSegmentModel.hpp"
void OSSIAAutomationElement::on_curveChanged()
{
    using namespace OSSIA;
    m_ossia_curve = Curve<float>::create();

    // For now we will assume that every segment is dynamic
    for(const auto& iscore_segment : m_iscore_autom->curve().segments())
    {
        auto linearSegment = CurveSegmentLinear<float>::create(m_ossia_curve);
        m_ossia_curve->addPoint(iscore_segment.start().x(), iscore_segment.start().y(), linearSegment);
        m_ossia_curve->addPoint(iscore_segment.end().x(), iscore_segment.end().y(), linearSegment);
    }

    //m_ossia_curve->setInitialValue((*m_iscore_autom->curve().segments().begin()).start().y());

}


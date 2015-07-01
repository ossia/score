#include "OSSIAAutomationElement.hpp"

#include <API/Headers/Editor/Automation.h>
#include <API/Headers/Editor/State.h>
#include <API/Headers/Editor/Message.h>
#include "../iscore-plugin-curve/Automation/AutomationModel.hpp"

#include <iscore/document/DocumentInterface.hpp>
OSSIAAutomationElement::OSSIAAutomationElement(const AutomationModel *element, QObject *parent):
    OSSIAProcessElement{parent},
    m_iscore_autom{element}
{
    qDebug() << "create";
    m_ossia_autom = OSSIA::Automation<double>::create([](
                                                      const OSSIA::TimeValue& position,
                                                      const OSSIA::TimeValue& date,
                                                      std::shared_ptr<OSSIA::State> state)
    {
        qDebug() << "automati callback" << double(position);
    });

    if(element->parent()->objectName() != QString("BaseConstraintModel"))
    {
        m_ossia_autom->getClock()->setExternal(true);
    }
    else
    {
        m_ossia_autom->getClock()->setSpeed(1.);
        m_ossia_autom->getClock()->setGranularity(250.);
    }


    connect(element, &AutomationModel::addressChanged,
            this, &OSSIAAutomationElement::on_addressChanged);
}

iscore::ElementPluginModel *OSSIAAutomationElement::clone(const QObject *element, QObject *parent) const
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    return nullptr;
}

iscore::ElementPluginModelType OSSIAAutomationElement::elementPluginId() const
{
    return staticPluginId();
}

void OSSIAAutomationElement::serialize(const VisitorVariant &) const
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
}

std::shared_ptr<OSSIA::TimeProcess> OSSIAAutomationElement::process() const
{
    return m_ossia_autom;
}

#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include "iscore2OSSIA.hpp"
#include "Protocols/OSSIADevice.hpp"
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

    // add "/test 0." message to Automation's start State
    OSSIA::Float zero(0.);
    auto first_start_message = OSSIA::Message::create(address, &zero);
    // TODO is this okay for removal?
    m_ossia_autom->getStartState()->stateElements().clear();
    m_ossia_autom->getStartState()->stateElements().push_back(first_start_message);

    // add "/test 1." message to Automation's end State
    OSSIA::Float one(1.);
    auto first_end_message = OSSIA::Message::create(address, &one);
    m_ossia_autom->getEndState()->stateElements().clear();
    m_ossia_autom->getEndState()->stateElements().push_back(first_end_message);
}

#include "OSSIAAutomationElement.hpp"

#include <API/Headers/Editor/Automation.h>
#include <API/Headers/Editor/State.h>
#include <API/Headers/Editor/Message.h>
#include "../iscore-plugin-curve/Automation/AutomationModel.hpp"
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
        qDebug() << "callback autom" << double(position);
    });
    m_ossia_autom->getClock()->setExternal(true);

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

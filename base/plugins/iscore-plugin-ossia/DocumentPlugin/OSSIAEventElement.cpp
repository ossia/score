#include "OSSIAEventElement.hpp"
#include <API/Headers/Editor/TimeEvent.h>
#include "../iscore-plugin-scenario/source/Document/Event/EventModel.hpp"
OSSIAEventElement::OSSIAEventElement(
        std::shared_ptr<OSSIA::TimeEvent> event,
        const EventModel* element,
        QObject* parent):
    iscore::ElementPluginModel{parent},
    m_event{event}
{

}


iscore::ElementPluginModel* OSSIAEventElement::clone(
        const QObject* element,
        QObject* parent) const
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    return nullptr;
}


std::shared_ptr<OSSIA::TimeEvent> OSSIAEventElement::event() const
{
    return m_event;
}

iscore::ElementPluginModelType OSSIAEventElement::elementPluginId() const
{
    return staticPluginId();
}

void OSSIAEventElement::serialize(const VisitorVariant&) const
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
}

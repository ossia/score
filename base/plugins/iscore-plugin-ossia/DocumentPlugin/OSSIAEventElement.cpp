#include "OSSIAEventElement.hpp"
#include <API/Headers/Editor/TimeEvent.h>
#include "../iscore-plugin-scenario/source/Document/Event/EventModel.hpp"
OSSIAEventElement::OSSIAEventElement(
        std::shared_ptr<OSSIA::TimeEvent> event,
        const EventModel* element,
        QObject* parent):
    m_event{event}
{
    // If the event is the first - last of the timenode
    // then it is already on it

    // Else we have to create it
    // What if the timenode has not been added to the scenario ? This should not happen in any case.
    // See also for the first and last event / timenodes (on the base constraint).

    // Note : we could add these elements on the OSSIAScenarioElement, when eventCreated() & al are emitted...
    // But how to manage base constraint then ? Here ?
    if(!element->parentScenario())
    {

    }

}


iscore::ElementPluginModel* OSSIAEventElement::clone(
        const QObject* element,
        QObject* parent) const
{
    qDebug() << Q_FUNC_INFO << "TODO";
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
    qDebug() << Q_FUNC_INFO << "TODO";
}

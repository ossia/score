#include "OSSIAEventElement.hpp"
#include "iscore2OSSIA.hpp"

#include <Document/Event/EventModel.hpp>
#include <API/Headers/Editor/TimeEvent.h>

OSSIAEventElement::OSSIAEventElement(
        std::shared_ptr<OSSIA::TimeEvent> event,
        const EventModel &element,
        QObject* parent):
    QObject{parent},
    m_iscore_event{element},
    m_event{event}
{
}

std::shared_ptr<OSSIA::TimeEvent> OSSIAEventElement::event() const
{
    return m_event;
}

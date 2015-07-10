#include "OSSIAEventElement.hpp"
#include <API/Headers/Editor/TimeEvent.h>
#include "../iscore-plugin-scenario/source/Document/Event/EventModel.hpp"
OSSIAEventElement::OSSIAEventElement(
        std::shared_ptr<OSSIA::TimeEvent> event,
        const EventModel &element,
        QObject* parent):
    QObject{parent},
    m_event{event}
{

}

std::shared_ptr<OSSIA::TimeEvent> OSSIAEventElement::event() const
{
    return m_event;
}


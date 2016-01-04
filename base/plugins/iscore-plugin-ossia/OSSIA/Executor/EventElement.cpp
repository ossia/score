#include <API/Headers/Editor/TimeEvent.h>
#include <Scenario/Document/Event/EventModel.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <QDebug>
#include <exception>

#include "Editor/Expression.h"
#include "EventElement.hpp"

namespace RecreateOnPlay
{
EventElement::EventElement(
        std::shared_ptr<OSSIA::TimeEvent> event,
        const EventModel &element,
        const DeviceList& deviceList,
        QObject* parent):
    QObject{parent},
    m_iscore_event{element},
    m_ossia_event{event},
    m_deviceList{deviceList}
{
    try
    {
        auto expr = iscore::convert::expression(m_iscore_event.condition(), m_deviceList);
        m_ossia_event->setExpression(expr);
    }
    catch(std::exception& e)
    {
        qDebug() << e.what();
        m_ossia_event->setExpression(OSSIA::Expression::create(true));
    }
}

std::shared_ptr<OSSIA::TimeEvent> EventElement::OSSIAEvent() const
{
    return m_ossia_event;
}
}

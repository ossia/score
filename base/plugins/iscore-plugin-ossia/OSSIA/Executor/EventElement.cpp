#include <ossia/editor/scenario/time_event.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <QDebug>
#include <exception>

#include <ossia/editor/expression/expression.hpp>
#include "EventElement.hpp"

namespace Engine { namespace Execution
{
EventElement::EventElement(
        std::shared_ptr<ossia::time_event> event,
        const Scenario::EventModel &element,
        const Device::DeviceList& deviceList,
        QObject* parent):
    QObject{parent},
    m_iscore_event{element},
    m_ossia_event{event},
    m_deviceList{deviceList}
{
    try
    {
        auto expr = Engine::iscore_to_ossia::expression(m_iscore_event.condition(), m_deviceList);
        m_ossia_event->setExpression(std::move(expr));
    }
    catch(std::exception& e)
    {
        qDebug() << e.what();
        m_ossia_event->setExpression(ossia::expressions::make_expression_true());
    }
}

std::shared_ptr<ossia::time_event> EventElement::OSSIAEvent() const
{
    return m_ossia_event;
}
} }

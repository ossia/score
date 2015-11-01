#include "OSSIAEventElement.hpp"
#include "iscore2OSSIA.hpp"

#include <Scenario/Document/Event/EventModel.hpp>
#include <API/Headers/Editor/TimeEvent.h>

OSSIAEventElement::OSSIAEventElement(
        std::shared_ptr<OSSIA::TimeEvent> event,
        const EventModel &element,
        const DeviceList& deviceList,
        QObject* parent):
    QObject{parent},
    m_iscore_event{element},
    m_ossia_event{event},
    m_deviceList{deviceList}
{
    con(m_iscore_event, &EventModel::conditionChanged,
        this, &OSSIAEventElement::on_conditionChanged);

    on_conditionChanged(m_iscore_event.condition());

}

std::shared_ptr<OSSIA::TimeEvent> OSSIAEventElement::OSSIAEvent() const
{
    return m_ossia_event;
}

void OSSIAEventElement::on_conditionChanged(const iscore::Condition& c)
try
{
    auto expr = iscore::convert::expression(c, m_deviceList);

    m_ossia_event->setExpression(expr);
}
catch(std::exception& e)
{
    qDebug() << e.what();
}

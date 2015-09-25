#include "OSSIAEventElement.hpp"
#include "iscore2OSSIA.hpp"

#include <Document/Event/EventModel.hpp>
#include <API/Headers/Editor/TimeEvent.h>

OSSIAEventElement::OSSIAEventElement(
        std::shared_ptr<OSSIA::TimeEvent> event,
        const EventModel &element,
        const DeviceList& deviceList,
        QObject* parent):
    QObject{parent},
    m_iscore_event{element},
    m_event{event},
    m_deviceList{deviceList}
{
    con(m_iscore_event, &EventModel::conditionChanged,
        this, &OSSIAEventElement::on_conditionChanged);

    on_conditionChanged(m_iscore_event.condition());

    m_event->setCallback(
                [] (OSSIA::TimeEvent::Status newStatus)
    {
        /*
        m_iscore_event.setStatus(static_cast<EventStatus>(newStatus));

        for(auto& state : the_event->states())
        {
            auto& iscore_state = m_iscore_scenario.state(state);

            switch(newStatus)
            {
                case OSSIA::TimeEvent::Status::NONE:
                    break;
                case OSSIA::TimeEvent::Status::PENDING:
                    break;
                case OSSIA::TimeEvent::Status::HAPPENED:
                {
                    // Stop the previous constraints clocks,
                    // start the next constraints clocks
                    if(iscore_state.previousConstraint())
                    {
                        stopConstraintExecution(iscore_state.previousConstraint());
                    }

                    if(iscore_state.nextConstraint())
                    {
                        startConstraintExecution(iscore_state.nextConstraint());
                    }
                    break;
                }

                case OSSIA::TimeEvent::Status::DISPOSED:
                {
                    // TODO disable the constraints graphically
                    break;
                }
                default:
                    ISCORE_TODO;
                    break;
            }
        }
        */
    });
}

std::shared_ptr<OSSIA::TimeEvent> OSSIAEventElement::event() const
{
    return m_event;
}

void OSSIAEventElement::on_conditionChanged(const iscore::Condition& c)
try
{
    auto expr = iscore::convert::expression(c, m_deviceList);

    m_event->setExpression(expr);
}
catch(std::exception& e)
{
    qDebug() << e.what();
}

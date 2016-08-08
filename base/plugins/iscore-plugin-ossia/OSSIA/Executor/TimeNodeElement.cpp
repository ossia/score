#include <ossia/editor/scenario/time_node.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <QDebug>
#include <exception>

#include <ossia/editor/expression/expression.hpp>
#include "TimeNodeElement.hpp"
#include "ConstraintElement.hpp"
#include <ossia/editor/state/state.hpp>

namespace Engine { namespace Execution
{
TimeNodeElement::TimeNodeElement(
        std::shared_ptr<ossia::time_node> ossia_tn,
        const Scenario::TimeNodeModel& element,
        const Device::DeviceList& devlist,
        QObject* parent):
    QObject{parent},
    m_ossia_node{ossia_tn},
    m_iscore_node{element},
    m_deviceList{devlist}
{
    if(element.trigger() && element.trigger()->active())
    {
        try
        {
            auto expr = Engine::iscore_to_ossia::expression(
                            element.trigger()->expression(),
                            m_deviceList);

            m_ossia_node->setExpression(std::move(expr));
        }
        catch(std::exception& e)
        {
            qDebug() << e.what();
            m_ossia_node->setExpression(ossia::expressions::make_expression_true());
        }
    }
    connect(m_iscore_node.trigger(), &Scenario::TriggerModel::triggeredByGui,
            this, [&] () {
        try {
            m_ossia_node->trigger();

            ossia::state accumulator;
            for(auto& event : m_ossia_node->timeEvents())
            {
                if(event->getStatus() == ossia::time_event::Status::HAPPENED)
                    ossia::flatten_and_filter(accumulator, event->getState());
            }
            accumulator.launch();
        }
        catch(...)
        {

        }
    });
}

std::shared_ptr<ossia::time_node> TimeNodeElement::OSSIATimeNode() const
{
    return m_ossia_node;
}

const Scenario::TimeNodeModel&TimeNodeElement::iscoreTimeNode() const
{
    return m_iscore_node;
}

} }

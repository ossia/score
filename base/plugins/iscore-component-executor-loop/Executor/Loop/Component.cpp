#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/editor/scenario/time_node.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <core/document/Document.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <algorithm>
#include <vector>
#include <Scenario/Document/Event/EventModel.hpp>

#include <ossia/editor/loop/loop.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/editor/state/state.hpp>
#include "Component.hpp"
#include <OSSIA/Executor/ConstraintElement.hpp>
#include <OSSIA/Executor/EventElement.hpp>
#include <OSSIA/Executor/ProcessElement.hpp>
#include <OSSIA/Executor/TimeNodeElement.hpp>
#include <OSSIA/Executor/StateElement.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>

#include <OSSIA/Executor/ExecutorContext.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>

namespace Process { class ProcessModel; }
class QObject;
namespace ossia {
class time_process;
}  // namespace OSSIA
#include <iscore/tools/SettableIdentifier.hpp>

namespace Loop
{
namespace RecreateOnPlay
{
Component::Component(
        ::RecreateOnPlay::ConstraintElement& parentConstraint,
        ::Loop::ProcessModel& element,
        const ::RecreateOnPlay::Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent):
    ::RecreateOnPlay::ProcessComponent_T<Loop::ProcessModel, ossia::loop>{
          parentConstraint, element, ctx, id, "LoopComponent", parent}
{
    ossia::time_value main_duration(iscore::convert::time(element.constraint().duration.defaultDuration()));

    auto loop = new ossia::loop(main_duration,
          [] (ossia::time_value, ossia::time_value,
              const ossia::state_element&) { },
          [this,&element] (ossia::time_event::Status newStatus) {

            element.startEvent().setStatus(static_cast<Scenario::ExecutionStatus>(newStatus));
            switch(newStatus)
            {
                case ossia::time_event::Status::NONE:
                    break;
                case ossia::time_event::Status::PENDING:
                    break;
                case ossia::time_event::Status::HAPPENED:
                    startConstraintExecution(m_ossia_constraint->iscoreConstraint().id());
                    break;
                case ossia::time_event::Status::DISPOSED:
                    break;
                default:
                    ISCORE_TODO;
                    break;
            }
    },
          [this,&element] (ossia::time_event::Status newStatus) {

        element.endEvent().setStatus(static_cast<Scenario::ExecutionStatus>(newStatus));
        switch(newStatus)
        {
            case ossia::time_event::Status::NONE:
                break;
            case ossia::time_event::Status::PENDING:
                break;
            case ossia::time_event::Status::HAPPENED:
                stopConstraintExecution(m_ossia_constraint->iscoreConstraint().id());
                break;
            case ossia::time_event::Status::DISPOSED:
                break;
            default:
                ISCORE_TODO;
                break;
        }

    }
    );

    m_ossia_process = loop;

    // TODO also states in BasEelement
    // TODO put graphical settings somewhere.
    auto startTN = loop->getPatternStartTimeNode();
    auto endTN = loop->getPatternEndTimeNode();
    auto startEV = *startTN->timeEvents().begin();
    auto endEV = *endTN->timeEvents().begin();

    using namespace ::RecreateOnPlay;
    m_ossia_startTimeNode = new TimeNodeElement{startTN, element.startTimeNode(), system().devices.list(), this};
    m_ossia_endTimeNode = new TimeNodeElement{endTN, element.endTimeNode(), system().devices.list(), this};

    m_ossia_startEvent = new EventElement{startEV, element.startEvent(), system().devices.list(), this};
    m_ossia_endEvent = new EventElement{endEV, element.endEvent(), system().devices.list(), this};


    m_ossia_startState = new StateElement{
            element.startState(),
            *startEV,
            system(),
            this};
    m_ossia_endState = new StateElement{
            element.endState(),
            *endEV,
            system(),
            this};

    m_ossia_constraint = new ConstraintElement{loop->getPatternTimeConstraint(), element.constraint(), system(), this};
}

Component::~Component()
{
}

void Component::stop()
{
    ProcessComponent::stop();
    process().constraint().duration.setPlayPercentage(0);
}

void Component::startConstraintExecution(const Id<Scenario::ConstraintModel>&)
{
    m_ossia_constraint->executionStarted();
}

void Component::stopConstraintExecution(const Id<Scenario::ConstraintModel>&)
{
    m_ossia_constraint->executionStopped();
}

}
}

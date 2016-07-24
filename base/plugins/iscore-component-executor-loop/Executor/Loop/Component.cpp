#include <Editor/TimeEvent.h>
#include <Editor/TimeNode.h>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <core/document/Document.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <algorithm>
#include <vector>
#include <Scenario/Document/Event/EventModel.hpp>
#include "Editor/Loop.h"
#include "Editor/TimeValue.h"
#include "Editor/State.h"
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
namespace OSSIA {
class TimeProcess;
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
    ::RecreateOnPlay::ProcessComponent_T<Loop::ProcessModel>{parentConstraint, element, ctx, id, "LoopComponent", parent}
{
    OSSIA::TimeValue main_duration(iscore::convert::time(element.constraint().duration.defaultDuration()));

    auto loop = OSSIA::Loop::create(main_duration,
          [] (OSSIA::TimeValue, OSSIA::TimeValue,
              const OSSIA::StateElement&) { },
          [this,&element] (OSSIA::TimeEvent::Status newStatus) {

            element.startEvent().setStatus(static_cast<Scenario::ExecutionStatus>(newStatus));
            switch(newStatus)
            {
                case OSSIA::TimeEvent::Status::NONE:
                    break;
                case OSSIA::TimeEvent::Status::PENDING:
                    break;
                case OSSIA::TimeEvent::Status::HAPPENED:
                    startConstraintExecution(m_ossia_constraint->iscoreConstraint().id());
                    break;
                case OSSIA::TimeEvent::Status::DISPOSED:
                    break;
                default:
                    ISCORE_TODO;
                    break;
            }
    },
          [this,&element] (OSSIA::TimeEvent::Status newStatus) {

        element.endEvent().setStatus(static_cast<Scenario::ExecutionStatus>(newStatus));
        switch(newStatus)
        {
            case OSSIA::TimeEvent::Status::NONE:
                break;
            case OSSIA::TimeEvent::Status::PENDING:
                break;
            case OSSIA::TimeEvent::Status::HAPPENED:
                stopConstraintExecution(m_ossia_constraint->iscoreConstraint().id());
                break;
            case OSSIA::TimeEvent::Status::DISPOSED:
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

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
#include <Loop/LoopProcessModel.hpp>
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
class StateElement;
class TimeProcess;
}  // namespace OSSIA
#include <iscore/tools/SettableIdentifier.hpp>

RecreateOnPlay::Loop::Component::Component(
        RecreateOnPlay::ConstraintElement& parentConstraint,
        ::Loop::ProcessModel& element,
        const Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent):
    ProcessComponent{parentConstraint, element, id, "LoopComponent", parent},
    m_ctx{ctx}
{
    OSSIA::TimeValue main_duration(iscore::convert::time(element.constraint().duration.defaultDuration()));

    auto loop = OSSIA::Loop::create(main_duration,
          [] (const OSSIA::TimeValue& t0,
              const OSSIA::TimeValue&,
              std::shared_ptr<OSSIA::StateElement>) {
    },
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
    auto startST = OSSIA::State::create();
    auto endST = OSSIA::State::create();

    startEV->addState(startST);
    endEV->addState(endST);


    m_ossia_startTimeNode = new TimeNodeElement{startTN, element.startTimeNode(),  m_ctx.devices.list(), this};
    m_ossia_endTimeNode = new TimeNodeElement{endTN, element.endTimeNode(), m_ctx.devices.list(), this};

    m_ossia_startEvent = new EventElement{startEV, element.startEvent(), m_ctx.devices.list(), this};
    m_ossia_endEvent = new EventElement{endEV, element.endEvent(), m_ctx.devices.list(), this};


    m_ossia_startState = new StateElement{
            element.startState(),
            startST,
            m_ctx,
            this};
    m_ossia_endState = new StateElement{
            element.endState(),
            endST,
            m_ctx,
            this};

    m_ossia_constraint = new ConstraintElement{loop->getPatternTimeConstraint(), element.constraint(), m_ctx, this};
}

RecreateOnPlay::Loop::Component::~Component()
{
}

void RecreateOnPlay::Loop::Component::stop()
{
    ProcessComponent::stop();
    static_cast< ::Loop::ProcessModel&>(m_iscore_process).constraint().duration.setPlayPercentage(0);
}

void RecreateOnPlay::Loop::Component::startConstraintExecution(const Id<Scenario::ConstraintModel>&)
{
    m_ossia_constraint->executionStarted();
}

void RecreateOnPlay::Loop::Component::stopConstraintExecution(const Id<Scenario::ConstraintModel>&)
{
    m_ossia_constraint->executionStopped();
}

const iscore::Component::Key&RecreateOnPlay::Loop::Component::key() const
{
    static iscore::Component::Key k("OSSIALoopElement");
    return k;
}

namespace RecreateOnPlay
{

namespace Loop
{
ComponentFactory::~ComponentFactory()
{

}

ProcessComponent* ComponentFactory::make(
        ConstraintElement& cst,
        Process::ProcessModel& proc,
        const Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent) const
{

    return new Component{
                cst,
                static_cast< ::Loop::ProcessModel&>(proc),
                ctx, id, parent};

}

const ComponentFactory::ConcreteFactoryKey&
ComponentFactory::concreteFactoryKey() const
{
    static ComponentFactory::ConcreteFactoryKey k("60e3b412-559d-4385-9a58-bdcd19bb9fa7");
    return k;
}

bool ComponentFactory::matches(
        Process::ProcessModel& proc,
        const DocumentPlugin&,
        const iscore::DocumentContext&) const
{
    return dynamic_cast< ::Loop::ProcessModel*>(&proc);
}
}
}

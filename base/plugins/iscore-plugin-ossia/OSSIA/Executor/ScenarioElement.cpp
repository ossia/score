#include <Editor/Scenario.h>
#include <Editor/TimeConstraint.h>
#include <Editor/TimeEvent.h>
#include <Editor/TimeNode.h>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <core/document/Document.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <QDebug>
#include <algorithm>
#include <vector>

#include "Editor/State.h"
#include "Editor/TimeValue.h"
#include <OSSIA/Executor/ConstraintElement.hpp>
#include <OSSIA/Executor/EventElement.hpp>
#include <OSSIA/Executor/ProcessElement.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <OSSIA/Executor/StateElement.hpp>
#include <OSSIA/Executor/TimeNodeElement.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include "ScenarioElement.hpp"

#include <Scenario/ExecutionChecker/CSPCoherencyCheckerInterface.hpp>
#include <Scenario/ExecutionChecker/CSPCoherencyCheckerList.hpp>
#include <Scenario/ExecutionChecker/CoherencyCheckerFactoryInterface.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <OSSIA/Executor/ExecutorContext.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>

namespace Process { class ProcessModel; }
class QObject;
namespace OSSIA {
class TimeProcess;
}  // namespace OSSIA


namespace RecreateOnPlay
{
ScenarioComponent::ScenarioComponent(
        ConstraintElement& parentConstraint,
        Scenario::ProcessModel& element,
        const Context& ctx,
        const Id<iscore::Component>& id,
        QObject* parent):
    ProcessComponent_T<Scenario::ProcessModel>{parentConstraint, element, ctx, id, "ScenarioComponent", parent},
    m_ctx{ctx}
{
    this->setObjectName("OSSIAScenarioElement");

    // Setup of the OSSIA API Part
    m_ossia_process = OSSIA::Scenario::create();

    // Create elements for the existing stuff. (e.g. start/ end timenode / event)
    for(const auto& timenode : element.timeNodes)
    {
        on_timeNodeCreated(timenode);
    }
    for(const auto& event : element.events)
    {
        on_eventCreated(event);
    }
    for(const auto& state : element.states)
    {
        on_stateCreated(state);
    }
    for(const auto& constraint : element.constraints)
    {
        on_constraintCreated(constraint);
    }

    if(auto fact = ctx.doc.app.components.factory<Scenario::CSPCoherencyCheckerList>().get())
    {
        m_checker = fact->make(element, ctx.doc.app, m_properties);
        if(m_checker)
        {
            m_properties.timenodes[Id<Scenario::TimeNodeModel>(0)].date = 0;
            m_checker->computeDisplacement(m_pastTn, m_properties);
        }
    }
}

void ScenarioComponent::stop()
{
    m_executingConstraints.clear();
    ProcessComponent::stop();
}

static void ScenarioConstraintCallback(
        OSSIA::TimeValue,
        OSSIA::TimeValue,
        std::shared_ptr<OSSIA::StateElement> element)
{

}


void ScenarioComponent::on_constraintCreated(const Scenario::ConstraintModel& const_constraint)
{
    auto& ossia_scenario = dynamic_cast<OSSIA::Scenario&>(*m_ossia_process.get());
    auto& cst = const_cast<Scenario::ConstraintModel&>(const_constraint);
    // TODO have a ConstraintPlayAspect to prevent this const_cast.
    ISCORE_ASSERT(m_ossia_timeevents.find(process().state(cst.startState()).eventId()) != m_ossia_timeevents.end());
    auto& ossia_sev = m_ossia_timeevents.at(process().state(cst.startState()).eventId());
    ISCORE_ASSERT(m_ossia_timeevents.find(process().state(cst.endState()).eventId()) != m_ossia_timeevents.end());
    auto& ossia_eev = m_ossia_timeevents.at(process().state(cst.endState()).eventId());

    auto ossia_cst = OSSIA::TimeConstraint::create(
                ScenarioConstraintCallback,
                ossia_sev->OSSIAEvent(),
                ossia_eev->OSSIAEvent(),
                iscore::convert::time(cst.duration.defaultDuration()),
                iscore::convert::time(cst.duration.minDuration()),
                iscore::convert::time(cst.duration.maxDuration()));


    ossia_scenario.addTimeConstraint(ossia_cst);

    // Create the mapping object
    auto elt = new ConstraintElement{ossia_cst, cst, m_ctx, this};
    m_ossia_constraints.insert({cst.id(), elt});
}

void ScenarioComponent::on_stateCreated(const Scenario::StateModel &iscore_state)
{
    ISCORE_ASSERT(m_ossia_timeevents.find(iscore_state.eventId()) != m_ossia_timeevents.end());
    auto ossia_ev = m_ossia_timeevents.at(iscore_state.eventId());

    // Create the mapping object
    auto root_state = OSSIA::State::create();
    ossia_ev->OSSIAEvent()->addState(root_state);

    auto state_elt = new StateElement{
            iscore_state,
            root_state,
            m_ctx,
            this};

    m_ossia_states.insert({iscore_state.id(), state_elt});
}

void ScenarioComponent::on_eventCreated(const Scenario::EventModel& const_ev)
{
    // TODO have a EventPlayAspect too
    auto& ev = const_cast<Scenario::EventModel&>(const_ev);
    ISCORE_ASSERT(m_ossia_timenodes.find(ev.timeNode()) != m_ossia_timenodes.end());
    auto ossia_tn = m_ossia_timenodes.at(ev.timeNode());

    auto ossia_ev = *ossia_tn->OSSIATimeNode()->emplace(
                ossia_tn->OSSIATimeNode()->timeEvents().begin(),
                OSSIA::TimeEvent::ExecutionCallback{});

    // Create the mapping object
    auto elt = new EventElement{ossia_ev, ev, m_ctx.devices.list(), this};
    m_ossia_timeevents.insert({ev.id(), elt});

    elt->OSSIAEvent()->setCallback([=] (OSSIA::TimeEvent::Status st) {
        return eventCallback(*elt, st);
    });
}

void ScenarioComponent::on_timeNodeCreated(const Scenario::TimeNodeModel& tn)
{
    auto& ossia_scenario = dynamic_cast<OSSIA::Scenario&>(*m_ossia_process.get());
    std::shared_ptr<OSSIA::TimeNode> ossia_tn;
    if(&tn == &process().startTimeNode())
    {
        ossia_tn = ossia_scenario.getStartTimeNode();
    }
    else
    {
        ossia_tn = OSSIA::TimeNode::create();
        ossia_scenario.addTimeNode(ossia_tn);
    }

    // Create the mapping object
    auto elt = new TimeNodeElement{ossia_tn, tn, m_ctx.devices.list(), this};
    m_ossia_timenodes.insert({tn.id(), elt});

    elt->OSSIATimeNode()->setCallback([=] () {
        return timeNodeCallback(elt, this->m_parent_constraint.OSSIAConstraint()->getDate());
    });
}

void ScenarioComponent::startConstraintExecution(const Id<Scenario::ConstraintModel>& id)
{
    auto& cst = process().constraints.at(id);
    if(m_executingConstraints.find(id) == m_executingConstraints.end())
        m_executingConstraints.insert(std::make_pair(cst.id(), &cst));

    m_ossia_constraints.at(id)->executionStarted();
}

void ScenarioComponent::disableConstraintExecution(const Id<Scenario::ConstraintModel>& id)
{
    auto& cst = process().constraints.at(id);
    cst.setExecutionState(Scenario::ConstraintExecutionState::Disabled);
}

void ScenarioComponent::stopConstraintExecution(const Id<Scenario::ConstraintModel>& id)
{
    m_executingConstraints.erase(id);
    m_ossia_constraints.at(id)->executionStopped();
}

void ScenarioComponent::eventCallback(
        EventElement& ev,
        OSSIA::TimeEvent::Status newStatus)
{
    auto the_event = const_cast<Scenario::EventModel*>(&ev.iscoreEvent());
    the_event->setStatus(static_cast<Scenario::ExecutionStatus>(newStatus));

    for(auto& state : the_event->states())
    {
        auto& iscore_state = process().states.at(state);

        if(auto& c = iscore_state.previousConstraint())
        {
            m_properties.constraints[c].status = static_cast<Scenario::ExecutionStatus>(newStatus);
        }

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
                if(iscore_state.nextConstraint())
                {
                    disableConstraintExecution(iscore_state.nextConstraint());
                }
                break;
            }
            default:
                ISCORE_TODO;
                break;
        }
    }
}

void ScenarioComponent::timeNodeCallback(
        TimeNodeElement* tn,
        OSSIA::TimeValue date)
{
    if(m_checker)
    {
        m_pastTn.push_back(tn->iscoreTimeNode().id());

        // Fix Timenode
        auto& curTnProp = m_properties.timenodes[tn->iscoreTimeNode().id()];
        curTnProp.date = double(date);
        curTnProp.date_max = curTnProp.date;
        curTnProp.date_min = curTnProp.date;
        curTnProp.status = Scenario::ExecutionStatus::Happened;

        // Fix previous constraints
        auto previousCstrs = Scenario::previousConstraints(tn->iscoreTimeNode(), process());

        for(auto& cstrId : previousCstrs)
        {
            auto& startTn = Scenario::startTimeNode(process().constraint(cstrId), process());
            auto& cstrProp = m_properties.constraints[cstrId];

            cstrProp.newMin.setMSecs(curTnProp.date - m_properties.timenodes[startTn.id()].date);
            cstrProp.newMax = cstrProp.newMin;

            cstrProp.status = Scenario::ExecutionStatus::Happened;
        }

        // Compute new values
        m_checker->computeDisplacement(m_pastTn, m_properties);

        // Update constraints
        for(auto& cstr : m_ossia_constraints)
        {
            auto ossiaCstr = cstr.second->OSSIAConstraint();

            auto tmin = m_properties.constraints[cstr.first].newMin.msec();
            ossiaCstr->setDurationMin(OSSIA::TimeValue{tmin});

            auto tmax = m_properties.constraints[cstr.first].newMax.msec();
            ossiaCstr->setDurationMax(OSSIA::TimeValue{tmax});
        }
    }
}
}

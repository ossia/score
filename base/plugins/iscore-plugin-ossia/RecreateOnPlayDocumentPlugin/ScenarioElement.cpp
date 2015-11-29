#include <API/Headers/Editor/Scenario.h>
#include <API/Headers/Editor/TimeConstraint.h>
#include <API/Headers/Editor/TimeEvent.h>
#include <API/Headers/Editor/TimeNode.h>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <boost/core/explicit_operator_bool.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore2OSSIA.hpp>
#include <QDebug>
#include <algorithm>
#include <vector>

#include "Editor/State.h"
#include "Editor/TimeValue.h"
#include "RecreateOnPlayDocumentPlugin/ConstraintElement.hpp"
#include "RecreateOnPlayDocumentPlugin/EventElement.hpp"
#include "RecreateOnPlayDocumentPlugin/ProcessElement.hpp"
#include "RecreateOnPlayDocumentPlugin/StateElement.hpp"
#include "RecreateOnPlayDocumentPlugin/TimeNodeElement.hpp"
#include "Scenario/Document/Constraint/ConstraintDurations.hpp"
#include "Scenario/Document/Event/EventModel.hpp"
#include "Scenario/Document/Event/ExecutionStatus.hpp"
#include "Scenario/Document/State/StateModel.hpp"
#include "Scenario/Document/TimeNode/TimeNodeModel.hpp"
#include "Scenario/Process/ScenarioModel.hpp"
#include "ScenarioElement.hpp"
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class Process;
class QObject;
namespace OSSIA {
class TimeProcess;
}  // namespace OSSIA


namespace RecreateOnPlay
{
ScenarioElement::ScenarioElement(
        ConstraintElement& parentConstraint,
        Scenario::ScenarioModel& element,
        QObject* parent):
    ProcessElement{parentConstraint, parent},
    m_iscore_scenario{element},
    m_deviceList{iscore::IDocument::documentFromObject(element)->model().pluginModel<DeviceDocumentPlugin>()->list()}
{
    this->setObjectName("OSSIAScenarioElement");

    // Setup of the OSSIA API Part
    m_ossia_scenario = OSSIA::Scenario::create();

    // Create elements for the existing stuff. (e.g. start/ end timenode / event)
    for(const auto& timenode : m_iscore_scenario.timeNodes)
    {
        on_timeNodeCreated(timenode);
    }
    for(const auto& event : m_iscore_scenario.events)
    {
        on_eventCreated(event);
    }
    for(const auto& state : m_iscore_scenario.states)
    {
        on_stateCreated(state);
    }
    for(const auto& constraint : m_iscore_scenario.constraints)
    {
        on_constraintCreated(constraint);
    }
}

std::shared_ptr<OSSIA::Scenario> ScenarioElement::scenario() const
{
    return m_ossia_scenario;
}

Process& ScenarioElement::iscoreProcess() const
{
    return m_iscore_scenario;
}

void ScenarioElement::stop()
{
    m_executingConstraints.clear();
    ProcessElement::stop();
}

std::shared_ptr<OSSIA::TimeProcess> ScenarioElement::OSSIAProcess() const
{
    return scenario();
}


static void ScenarioConstraintCallback(const OSSIA::TimeValue&,
                               const OSSIA::TimeValue&,
                               std::shared_ptr<OSSIA::StateElement> element)
{

}


void ScenarioElement::on_constraintCreated(const ConstraintModel& const_constraint)
{
    auto& cst = const_cast<ConstraintModel&>(const_constraint);
    // TODO have a ConstraintPlayAspect to prevent this const_cast.
    ISCORE_ASSERT(m_ossia_timeevents.find(m_iscore_scenario.state(cst.startState()).eventId()) != m_ossia_timeevents.end());
    auto& ossia_sev = m_ossia_timeevents.at(m_iscore_scenario.state(cst.startState()).eventId());
    ISCORE_ASSERT(m_ossia_timeevents.find(m_iscore_scenario.state(cst.endState()).eventId()) != m_ossia_timeevents.end());
    auto& ossia_eev = m_ossia_timeevents.at(m_iscore_scenario.state(cst.endState()).eventId());

    auto ossia_cst = OSSIA::TimeConstraint::create(
                ScenarioConstraintCallback,
                ossia_sev->OSSIAEvent(),
                ossia_eev->OSSIAEvent(),
                iscore::convert::time(cst.duration.defaultDuration()),
                iscore::convert::time(cst.duration.minDuration()),
                iscore::convert::time(cst.duration.maxDuration()));


    m_ossia_scenario->addTimeConstraint(ossia_cst);

    // Create the mapping object
    auto elt = new ConstraintElement{ossia_cst, cst, this};
    m_ossia_constraints.insert({cst.id(), elt});
}

void ScenarioElement::on_stateCreated(const StateModel &iscore_state)
{
    ISCORE_ASSERT(m_ossia_timeevents.find(iscore_state.eventId()) != m_ossia_timeevents.end());
    auto ossia_ev = m_ossia_timeevents.at(iscore_state.eventId());

    // Create the mapping object
    auto root_state = OSSIA::State::create();
    ossia_ev->OSSIAEvent()->addState(root_state);

    auto state_elt = new StateElement{
            iscore_state,
            root_state,
            m_deviceList,
            this};

    m_ossia_states.insert({iscore_state.id(), state_elt});
}

void ScenarioElement::on_eventCreated(const EventModel& const_ev)
{
    // TODO have a EventPlayAspect too
    auto& ev = const_cast<EventModel&>(const_ev);
    ISCORE_ASSERT(m_ossia_timenodes.find(ev.timeNode()) != m_ossia_timenodes.end());
    auto ossia_tn = m_ossia_timenodes.at(ev.timeNode());

    auto ossia_ev = *ossia_tn->OSSIATimeNode()->emplace(
                ossia_tn->OSSIATimeNode()->timeEvents().begin(),
                OSSIA::TimeEvent::ExecutionCallback{});

    // Create the mapping object
    auto elt = new EventElement{ossia_ev, ev, m_deviceList, this};
    m_ossia_timeevents.insert({ev.id(), elt});

    elt->OSSIAEvent()->setCallback([=] (OSSIA::TimeEvent::Status st) {
        return eventCallback(*elt, st);
    });
}

void ScenarioElement::on_timeNodeCreated(const TimeNodeModel& tn)
{
    std::shared_ptr<OSSIA::TimeNode> ossia_tn;
    if(&tn == &m_iscore_scenario.startTimeNode())
    {
        ossia_tn = m_ossia_scenario->getStartTimeNode();
    }
    else if(&tn == &m_iscore_scenario.endTimeNode())
    {
        ossia_tn = m_ossia_scenario->getEndTimeNode();
    }
    else
    {
        ossia_tn = OSSIA::TimeNode::create();
        m_ossia_scenario->addTimeNode(ossia_tn);
    }

    // Create the mapping object
    auto elt = new TimeNodeElement{ossia_tn, tn, m_deviceList, this};
    m_ossia_timenodes.insert({tn.id(), elt});
}

void ScenarioElement::startConstraintExecution(const Id<ConstraintModel>& id)
{
    auto& cst = m_iscore_scenario.constraints.at(id);
    m_executingConstraints.insert(&cst);

    m_ossia_constraints.at(id)->executionStarted();
}

void ScenarioElement::stopConstraintExecution(const Id<ConstraintModel>& id)
{
    m_executingConstraints.remove(id);
    m_ossia_constraints.at(id)->executionStopped();
}

void ScenarioElement::eventCallback(
        EventElement& ev,
        OSSIA::TimeEvent::Status newStatus)
{
    auto the_event = const_cast<EventModel*>(&ev.iscoreEvent());
    the_event->setStatus(static_cast<ExecutionStatus>(newStatus));

    for(auto& state : the_event->states())
    {
        auto& iscore_state = m_iscore_scenario.states.at(state);

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
}
}

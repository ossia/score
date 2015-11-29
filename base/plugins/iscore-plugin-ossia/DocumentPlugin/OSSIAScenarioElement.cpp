#include <API/Headers/Editor/Scenario.h>
#include <API/Headers/Editor/TimeConstraint.h>
#include <API/Headers/Editor/TimeEvent.h>
#include <API/Headers/Editor/TimeNode.h>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <boost/core/explicit_operator_bool.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QDebug>
#include <algorithm>
#include <utility>
#include <vector>

#include "DocumentPlugin/OSSIAConstraintElement.hpp"
#include "DocumentPlugin/OSSIAEventElement.hpp"
#include "DocumentPlugin/OSSIAProcessElement.hpp"
#include "DocumentPlugin/OSSIAStateElement.hpp"
#include "DocumentPlugin/OSSIATimeNodeElement.hpp"
#include "Editor/State.h"
#include "Editor/TimeValue.h"
#include "OSSIAScenarioElement.hpp"
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>
#include "iscore2OSSIA.hpp"

class Process;
class QObject;
namespace OSSIA {
class TimeProcess;
}  // namespace OSSIA

OSSIAScenarioElement::OSSIAScenarioElement(
        OSSIAConstraintElement& parentConstraint,
        Scenario::ScenarioModel& element,
        QObject* parent):
    OSSIAProcessElement{parentConstraint, parent},
    m_iscore_scenario{element},
    m_deviceList{iscore::IDocument::documentFromObject(element)->model().pluginModel<DeviceDocumentPlugin>()->list()}
{
    this->setObjectName("OSSIAScenarioElement");

    // Setup of the OSSIA API Part
    m_ossia_scenario = OSSIA::Scenario::create();

    // Link with i-score
    con(element.constraints, &NotifyingMap<ConstraintModel>::added,
            this, &OSSIAScenarioElement::on_constraintCreated);
    con(element.states, &NotifyingMap<StateModel>::added,
            this, &OSSIAScenarioElement::on_stateCreated);
    con(element.events, &NotifyingMap<EventModel>::added,
            this, &OSSIAScenarioElement::on_eventCreated);
    con(element.timeNodes, &NotifyingMap<TimeNodeModel>::added,
        this, &OSSIAScenarioElement::on_timeNodeCreated);

    con(element.constraints, &NotifyingMap<ConstraintModel>::removed,
            this, &OSSIAScenarioElement::on_constraintRemoved);
    con(element.states, &NotifyingMap<StateModel>::removed,
            this, &OSSIAScenarioElement::on_stateRemoved);
    con(element.events, &NotifyingMap<EventModel>::removed,
            this, &OSSIAScenarioElement::on_eventRemoved);
    con(element.timeNodes, &NotifyingMap<TimeNodeModel>::removed,
        this, &OSSIAScenarioElement::on_timeNodeRemoved);

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

std::shared_ptr<OSSIA::Scenario> OSSIAScenarioElement::scenario() const
{
    return m_ossia_scenario;
}

Process& OSSIAScenarioElement::iscoreProcess() const
{
    return m_iscore_scenario;
}

void OSSIAScenarioElement::stop()
{
    m_executingConstraints.clear();
    // FIXME should this call OSSIAProcessElement::stop() ???
}

void OSSIAScenarioElement::recreate()
{
    for(const auto& elt : m_ossia_states)
    {
        elt.second->recreate();
    }
    for(const auto& elt : m_ossia_timeevents)
    {
        elt.second->recreate();
    }
    for(const auto& elt : m_ossia_timenodes)
    {
        elt.second->recreate();
    }
    for(const auto& elt : m_ossia_constraints)
    {
        elt.second->recreate();
    }
}

void OSSIAScenarioElement::clear()
{
    for(const auto& elt : m_ossia_states)
    {
        elt.second->clear();
    }
    for(const auto& elt : m_ossia_timeevents)
    {
        elt.second->clear();
    }
    for(const auto& elt : m_ossia_timenodes)
    {
        elt.second->clear();
    }
    for(const auto& elt : m_ossia_constraints)
    {
        elt.second->clear();
    }
}

std::shared_ptr<OSSIA::TimeProcess> OSSIAScenarioElement::OSSIAProcess() const
{
    return scenario();
}


static void ScenarioConstraintCallback(const OSSIA::TimeValue&,
                               const OSSIA::TimeValue&,
                               std::shared_ptr<OSSIA::StateElement> element)
{

}


void OSSIAScenarioElement::on_constraintCreated(const ConstraintModel& const_constraint)
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
    auto elt = new OSSIAConstraintElement{ossia_cst, cst, this};
    m_ossia_constraints.insert({cst.id(), elt});
}

void OSSIAScenarioElement::on_stateCreated(const StateModel &iscore_state)
{
    ISCORE_ASSERT(m_ossia_timeevents.find(iscore_state.eventId()) != m_ossia_timeevents.end());
    auto ossia_ev = m_ossia_timeevents.at(iscore_state.eventId());

    // Create the mapping object
    auto root_state = OSSIA::State::create();
    ossia_ev->OSSIAEvent()->addState(root_state);

    auto state_elt = new OSSIAStateElement{
            iscore_state,
            root_state,
            m_deviceList,
            this};

    m_ossia_states.insert({iscore_state.id(), state_elt});
}

void OSSIAScenarioElement::on_eventCreated(const EventModel& const_ev)
{
    // TODO have a EventPlayAspect too
    auto& ev = const_cast<EventModel&>(const_ev);
    ISCORE_ASSERT(m_ossia_timenodes.find(ev.timeNode()) != m_ossia_timenodes.end());
    auto ossia_tn = m_ossia_timenodes.at(ev.timeNode());

    auto ossia_ev = *ossia_tn->OSSIATimeNode()->emplace(
                ossia_tn->OSSIATimeNode()->timeEvents().begin(),
                OSSIA::TimeEvent::ExecutionCallback{});

    // Create the mapping object
    auto elt = new OSSIAEventElement{ossia_ev, ev, m_deviceList, this};
    m_ossia_timeevents.insert({ev.id(), elt});

    elt->OSSIAEvent()->setCallback([=] (OSSIA::TimeEvent::Status st) {
        return eventCallback(*elt, st);
    });
}

void OSSIAScenarioElement::on_timeNodeCreated(const TimeNodeModel& tn)
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
    auto elt = new OSSIATimeNodeElement{ossia_tn, tn, m_deviceList, this};
    m_ossia_timenodes.insert({tn.id(), elt});
}

void OSSIAScenarioElement::on_constraintRemoved(const ConstraintModel& iscore_cstr)
{
    auto it = m_ossia_constraints.find(iscore_cstr.id());
    auto cst = (*it).second;

    auto ref = cst->OSSIAConstraint();

    // API Cleanup
    m_ossia_scenario->removeTimeConstraint(cst->OSSIAConstraint());

    // Remove the constraint from the events
    auto& startEventNextConstraints = ref->getStartEvent()->nextTimeConstraints();
    startEventNextConstraints.erase(std::remove(startEventNextConstraints.begin(),
                                                startEventNextConstraints.end(),
                                                ref),
                                    startEventNextConstraints.end());
    auto& endEventPreviousConstraints = ref->getEndEvent()->previousTimeConstraints();
    endEventPreviousConstraints.erase(std::remove(endEventPreviousConstraints.begin(),
                                                  endEventPreviousConstraints.end(),
                                                  ref),
                                      endEventPreviousConstraints.end());

    m_ossia_constraints.erase(it);
    delete cst;
}

void OSSIAScenarioElement::on_stateRemoved(const StateModel& iscore_state)
{
    auto it = m_ossia_states.find(iscore_state.id());
    ISCORE_ASSERT(it != m_ossia_states.end());
    auto state_elt = (*it).second;

    auto ev_it = m_ossia_timeevents.find(iscore_state.eventId());
    if(ev_it != m_ossia_timeevents.end())
    {
        OSSIAEventElement* ev = (*ev_it).second;
        ev->OSSIAEvent()->removeState(state_elt->OSSIAState());
    }

    m_ossia_states.erase(it);
    delete state_elt;
}

void OSSIAScenarioElement::on_eventRemoved(const EventModel& iscore_ev)
{
    auto ev_it = m_ossia_timeevents.find(iscore_ev.id());
    ISCORE_ASSERT(ev_it != m_ossia_timeevents.end());
    OSSIAEventElement* ev = (*ev_it).second;

    auto tn_it = m_ossia_timenodes.find(iscore_ev.timeNode());
    if(tn_it != m_ossia_timenodes.end())
    {
        OSSIATimeNodeElement* tn = (*tn_it).second;

        // Cleanup the timenode
        auto& timeEvents = tn->OSSIATimeNode()->timeEvents();
        timeEvents.erase( std::remove( timeEvents.begin(), timeEvents.end(), ev->OSSIAEvent()), timeEvents.end());
    }

    m_ossia_timeevents.erase(ev_it);
    delete ev;
}

void OSSIAScenarioElement::on_timeNodeRemoved(const TimeNodeModel& iscore_tn)
{
    auto tn_it = m_ossia_timenodes.find(iscore_tn.id());
    OSSIATimeNodeElement* tn = (*tn_it).second;

    m_ossia_scenario->removeTimeNode(tn->OSSIATimeNode());
    // Deletion will be part of the TimeNodeModel* delete.

    m_ossia_timenodes.erase(tn_it);
    delete tn;
}

void OSSIAScenarioElement::startConstraintExecution(const Id<ConstraintModel>& id)
{
    auto& cst = m_iscore_scenario.constraints.at(id);
    m_executingConstraints.insert(&cst);

    m_ossia_constraints.at(id)->executionStarted();
}

void OSSIAScenarioElement::stopConstraintExecution(const Id<ConstraintModel>& id)
{
    m_executingConstraints.remove(id);
    m_ossia_constraints.at(id)->executionStopped();
}

void OSSIAScenarioElement::eventCallback(
        OSSIAEventElement& ev,
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

#include "OSSIAScenarioElement.hpp"

#include <API/Headers/Editor/Scenario.h>
#include <API/Headers/Editor/TimeConstraint.h>
#include <API/Headers/Editor/TimeEvent.h>
#include <API/Headers/Editor/TimeNode.h>

#include "iscore2OSSIA.hpp"
#include "OSSIA2iscore.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include "../iscore-plugin-deviceexplorer/Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp"

OSSIAScenarioElement::OSSIAScenarioElement(
        OSSIAConstraintElement *parentConstraint,
        const ScenarioModel* element,
        QObject* parent):
    OSSIAProcessElement{parent},
    m_parent_constraint{parentConstraint},
    m_iscore_scenario{element},
    m_deviceList{iscore::IDocument::documentFromObject(element)->model().pluginModel<DeviceDocumentPlugin>()->list()}
{
    this->setObjectName("OSSIAScenarioElement");

    // Setup of the OSSIA API Part
    m_ossia_scenario = OSSIA::Scenario::create();

    // Link with i-score
    con(element->constraints, &NotifyingMap<ConstraintModel>::added,
            this, &OSSIAScenarioElement::on_constraintCreated);
    con(element->states, &NotifyingMap<StateModel>::added,
            this, &OSSIAScenarioElement::on_stateCreated);
    con(element->events, &NotifyingMap<EventModel>::added,
            this, &OSSIAScenarioElement::on_eventCreated);
    con(element->timeNodes, &NotifyingMap<TimeNodeModel>::added,
        this, &OSSIAScenarioElement::on_timeNodeCreated);

    con(element->constraints, &NotifyingMap<ConstraintModel>::removed,
            this, &OSSIAScenarioElement::on_constraintRemoved);
    con(element->states, &NotifyingMap<StateModel>::removed,
            this, &OSSIAScenarioElement::on_stateRemoved);
    con(element->events, &NotifyingMap<EventModel>::removed,
            this, &OSSIAScenarioElement::on_eventRemoved);
    con(element->timeNodes, &NotifyingMap<TimeNodeModel>::removed,
        this, &OSSIAScenarioElement::on_timeNodeRemoved);

    // Create elements for the existing stuff. (e.g. start/ end timenode / event)
    for(const auto& timenode : m_iscore_scenario->timeNodes)
    {
        on_timeNodeCreated(timenode);
    }
    for(const auto& event : m_iscore_scenario->events)
    {
        on_eventCreated(event);
    }
    for(const auto& state : m_iscore_scenario->states)
    {
        on_stateCreated(state);
    }
    for(const auto& constraint : m_iscore_scenario->constraints)
    {
        on_constraintCreated(constraint);
    }
}


std::shared_ptr<OSSIA::Scenario> OSSIAScenarioElement::scenario() const
{
    return m_ossia_scenario;
}

const Process *OSSIAScenarioElement::iscoreProcess() const
{
    return m_iscore_scenario;
}

void OSSIAScenarioElement::stop()
{
    m_executingConstraints.clear();
}

std::shared_ptr<OSSIA::TimeProcess> OSSIAScenarioElement::process() const
{
    return scenario();
}


void OSSIAScenarioElement::on_constraintCreated(const ConstraintModel& const_constraint)
{
    auto& cst = const_cast<ConstraintModel&>(const_constraint);
    // TODO have a ConstraintPlayAspect
    ISCORE_ASSERT(m_ossia_timeevents.find(m_iscore_scenario->state(cst.startState()).eventId()) != m_ossia_timeevents.end());
    auto& ossia_sev = m_ossia_timeevents.at(m_iscore_scenario->state(cst.startState()).eventId());
    ISCORE_ASSERT(m_ossia_timeevents.find(m_iscore_scenario->state(cst.endState()).eventId()) != m_ossia_timeevents.end());
    auto& ossia_eev = m_ossia_timeevents.at(m_iscore_scenario->state(cst.endState()).eventId());

    auto ossia_cst = OSSIA::TimeConstraint::create([=,iscore_constraint=&cst](
                                                   const OSSIA::TimeValue& position,
                                                   const OSSIA::TimeValue& date,
                                                   std::shared_ptr<OSSIA::StateElement> state) {
        auto currentTime = OSSIA::convert::time(date);
        iscore_constraint->duration.setPlayPercentage(currentTime / iscore_constraint->duration.maxDuration());

        state->launch();
    },
                ossia_sev->event(),
                ossia_eev->event(),
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
    auto root_state = iscore::convert::state(iscore_state.states().rootNode(), m_deviceList);
    auto state_elt = new OSSIAStateElement{
            iscore_state,
            root_state,
            this};

    ossia_ev->event()->addState(root_state);


    con(iscore_state, &StateModel::statesUpdated, this,
            [=] () {
        // OPTIMIZEME
        state_elt->rootState()->stateElements().clear();
        for(auto& elt : state_elt->iscoreState().states().rootNode().children())
        {
            state_elt->rootState()->stateElements().push_back(iscore::convert::state(*elt, m_deviceList));
        }

    } );

    m_ossia_states.insert({iscore_state.id(), state_elt});
}

void OSSIAScenarioElement::on_eventCreated(const EventModel& const_ev)
{
    // TODO have a EventPlayAspect too
    auto& ev = const_cast<EventModel&>(const_ev);
    ISCORE_ASSERT(m_ossia_timenodes.find(ev.timeNode()) != m_ossia_timenodes.end());
    auto ossia_tn = m_ossia_timenodes.at(ev.timeNode());

    auto ossia_ev = *ossia_tn->timeNode()->emplace(ossia_tn->timeNode()->timeEvents().begin(),
                                                   [=,the_event=&ev] (OSSIA::TimeEvent::Status newStatus)
    {
        the_event->setStatus(static_cast<EventStatus>(newStatus));

        for(auto& state : the_event->states())
        {
            auto& iscore_state = m_iscore_scenario->state(state);

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
                        m_executingConstraints.remove(iscore_state.previousConstraint());

                    if(iscore_state.nextConstraint())
                    {
                        auto& cst = m_iscore_scenario->constraint(iscore_state.nextConstraint());
                        m_executingConstraints.insert(&cst);
                        cst.duration.setPlayPercentage(0);
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
    });

    connect(&ev, &EventModel::conditionChanged,
            this, [=] (const iscore::Condition& c) {
        try {
            auto expr = iscore::convert::expression(c, m_deviceList);

            ossia_ev->setExpression(expr);
        }
        catch(std::exception& e)
        {
            qDebug() << e.what();
        }
    });

    // Create the mapping object
    auto elt = new OSSIAEventElement{ossia_ev, ev, this};
    m_ossia_timeevents.insert({ev.id(), elt});
}

void OSSIAScenarioElement::on_timeNodeCreated(const TimeNodeModel& tn)
{
    std::shared_ptr<OSSIA::TimeNode> ossia_tn;
    if(&tn == &m_iscore_scenario->startTimeNode())
    {
        ossia_tn = m_ossia_scenario->getStartNode();
    }
    else if(&tn == &m_iscore_scenario->endTimeNode())
    {
        ossia_tn = m_ossia_scenario->getEndNode();
    }
    else
    {
        ossia_tn = OSSIA::TimeNode::create();
        m_ossia_scenario->addTimeNode(ossia_tn);
    }

    // Create the mapping object
    auto elt = new OSSIATimeNodeElement{ossia_tn, tn, this};
    m_ossia_timenodes.insert({tn.id(), elt});
}

void OSSIAScenarioElement::on_constraintRemoved(const ConstraintModel& iscore_cstr)
{
    auto it = m_ossia_constraints.find(iscore_cstr.id());
    auto cst = (*it).second;

    auto ref = cst->constraint();

    // API Cleanup
    m_ossia_scenario->removeTimeConstraint(cst->constraint());

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
        ev->event()->removeState(state_elt->rootState());
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
        auto& timeEvents = tn->timeNode()->timeEvents();
        timeEvents.erase( std::remove( timeEvents.begin(), timeEvents.end(), ev->event()), timeEvents.end());
    }

    m_ossia_timeevents.erase(ev_it);
    delete ev;
}

void OSSIAScenarioElement::on_timeNodeRemoved(const TimeNodeModel& iscore_tn)
{
    auto tn_it = m_ossia_timenodes.find(iscore_tn.id());
    OSSIATimeNodeElement* tn = (*tn_it).second;

    m_ossia_scenario->removeTimeNode(tn->timeNode());
    // Deletion will be part of the TimeNodeModel* delete.

    m_ossia_timenodes.erase(tn_it);
    delete tn;
}

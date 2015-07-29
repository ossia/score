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
    m_deviceList{iscore::IDocument::documentFromObject(element)->model()->pluginModel<DeviceDocumentPlugin>()->list()}
{
    this->setObjectName("OSSIAScenarioElement");

    // Setup of the OSSIA API Part
    m_ossia_scenario = OSSIA::Scenario::create();

    // Link with i-score
    connect(element, &ScenarioModel::constraintCreated,
            this, &OSSIAScenarioElement::on_constraintCreated);
    connect(element, &ScenarioModel::stateCreated,
            this, &OSSIAScenarioElement::on_stateCreated);
    connect(element, &ScenarioModel::eventCreated,
            this, &OSSIAScenarioElement::on_eventCreated);
    connect(element, &ScenarioModel::timeNodeCreated,
            this, &OSSIAScenarioElement::on_timeNodeCreated);

    connect(element, &ScenarioModel::constraintRemoved,
            this, &OSSIAScenarioElement::on_constraintRemoved);
    connect(element, &ScenarioModel::stateRemoved,
            this, &OSSIAScenarioElement::on_stateRemoved);
    connect(element, &ScenarioModel::eventRemoved_before,
            this, &OSSIAScenarioElement::on_eventRemoved);
    connect(element, &ScenarioModel::timeNodeRemoved,
            this, &OSSIAScenarioElement::on_timeNodeRemoved);

    // Create elements for the existing stuff. (e.g. start/ end timenode / event)
    for(const auto& timenode : m_iscore_scenario->timeNodes())
    {
        on_timeNodeCreated(timenode);
    }
    for(const auto& event : m_iscore_scenario->events())
    {
        on_eventCreated(event);
    }
    for(const auto& state : m_iscore_scenario->states())
    {
        on_stateCreated(state);
    }
    for(const auto& constraint : m_iscore_scenario->constraints())
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
    m_previousExecutionDate = TimeValue::zero();
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
    Q_ASSERT(m_ossia_timeevents.find(m_iscore_scenario->state(cst.startState()).eventId()) != m_ossia_timeevents.end());
    auto& ossia_sev = m_ossia_timeevents.at(m_iscore_scenario->state(cst.startState()).eventId());
    Q_ASSERT(m_ossia_timeevents.find(m_iscore_scenario->state(cst.endState()).eventId()) != m_ossia_timeevents.end());
    auto& ossia_eev = m_ossia_timeevents.at(m_iscore_scenario->state(cst.endState()).eventId());

    auto ossia_cst = OSSIA::TimeConstraint::create([=,iscore_constraint=&cst](
                                                   const OSSIA::TimeValue& position, // TODO should not be a timevalue but a double
                                                   const OSSIA::TimeValue& date,
                                                   std::shared_ptr<OSSIA::StateElement> state) {
        auto currentTime = OSSIA::convert::time(date);
        iscore_constraint->setPlayDuration(currentTime);

        state->launch();
    },
                ossia_sev->event(),
                ossia_eev->event(),
                iscore::convert::time(cst.defaultDuration()),
                iscore::convert::time(cst.minDuration()),
                iscore::convert::time(cst.maxDuration()));


    m_ossia_scenario->addTimeConstraint(ossia_cst);

    // Create the mapping object
    auto elt = new OSSIAConstraintElement{ossia_cst, cst, this};
    m_ossia_constraints.insert({cst.id(), elt});
}

void OSSIAScenarioElement::on_stateCreated(const StateModel &iscore_state)
{
    Q_ASSERT(m_ossia_timeevents.find(iscore_state.eventId()) != m_ossia_timeevents.end());
    auto ossia_ev = m_ossia_timeevents.at(iscore_state.eventId());

    // Create the mapping object
    auto state_elt = new OSSIAStateElement{iscore_state, this};
    for(auto& st_val : iscore_state.states())
    {
        auto ossia_st = iscore::convert::state(st_val, m_deviceList);
        ossia_ev->event()->addState(ossia_st);

        state_elt->addState(st_val, ossia_st);
    }

    connect(&iscore_state, &StateModel::stateAdded, this,
            [=] (const iscore::State& st_val) {
        auto ossia_st = iscore::convert::state(st_val, m_deviceList);
        ossia_ev->event()->addState(ossia_st);

        state_elt->addState(st_val, ossia_st);

    } );
    connect(&iscore_state, &StateModel::stateRemoved, this,
            [=] (const iscore::State& st_val) {
        ossia_ev->event()->removeState(state_elt->states().at(st_val));
        state_elt->removeState(st_val);
    });
    connect(&iscore_state, &StateModel::statesReplaced, this,
            [=] () {
        for(auto& states : state_elt->states())
        {
            ossia_ev->event()->removeState(states.second);
            state_elt->removeState(states.first);
        }

        for(auto& st_val : state_elt->iscoreState().states())
        {
            auto ossia_st = iscore::convert::state(st_val, m_deviceList);
            ossia_ev->event()->addState(ossia_st);

            state_elt->addState(st_val, ossia_st);
        }
    });

    m_ossia_states.insert({iscore_state.id(), state_elt});
}

void OSSIAScenarioElement::on_eventCreated(const EventModel& const_ev)
{
    // TODO have a EventPlayAspect too
    auto& ev = const_cast<EventModel&>(const_ev);
    Q_ASSERT(m_ossia_timenodes.find(ev.timeNode()) != m_ossia_timenodes.end());
    auto ossia_tn = m_ossia_timenodes.at(ev.timeNode());

    auto ossia_ev = *ossia_tn->timeNode()->emplace(ossia_tn->timeNode()->timeEvents().begin(),
                                                   [=,the_event=&ev] (OSSIA::TimeEvent::Status newStatus)
    {
        the_event->setStatus(static_cast<EventStatus>(newStatus));

        for(auto& state : the_event->states())
        {
            auto& iscore_state = m_iscore_scenario->state(state);
            // qDebug() << "AZEAZEAZE" << 1234124 << 2143 << "klm" << (int) newStatus;

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
                        cst.setPlayDuration(TimeValue::zero());
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

void OSSIAScenarioElement::on_constraintRemoved(const id_type<ConstraintModel>& id)
{
    auto it = m_ossia_constraints.find(id);
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

void OSSIAScenarioElement::on_stateRemoved(const id_type<StateModel> &id)
{
    auto it = m_ossia_states.find(id);
    Q_ASSERT(it != m_ossia_states.end());
    auto state_elt = (*it).second;

    auto ev_it = m_ossia_timeevents.find(m_iscore_scenario->state(id).eventId());
    if(ev_it != m_ossia_timeevents.end())
    {
        OSSIAEventElement* ev = (*ev_it).second;
        for(auto& state : state_elt->states())
            ev->event()->removeState(state.second);
    }

    m_ossia_states.erase(it);
    delete state_elt;
}

void OSSIAScenarioElement::on_eventRemoved(const id_type<EventModel>& id)
{
    auto ev_it = m_ossia_timeevents.find(id);
    Q_ASSERT(ev_it != m_ossia_timeevents.end());
    OSSIAEventElement* ev = (*ev_it).second;

    auto tn_it = m_ossia_timenodes.find(m_iscore_scenario->event(id).timeNode());
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

void OSSIAScenarioElement::on_timeNodeRemoved(const id_type<TimeNodeModel>& id)
{
    auto tn_it = m_ossia_timenodes.find(id);
    OSSIATimeNodeElement* tn = (*tn_it).second;

    m_ossia_scenario->removeTimeNode(tn->timeNode());
    // Deletion will be part of the TimeNodeModel* delete.

    m_ossia_timenodes.erase(tn_it);
    delete tn;
}

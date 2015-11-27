#include "DisplayedElementsModel.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

#include <Scenario/Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <iscore/plugins/customfactory/FactoryInterface.hpp>

struct DisplayedElementsContainer {
    QPointer<const ConstraintModel> constraint{};
    QPointer<const StateModel> startState{};
    QPointer<const StateModel> endState{};
    QPointer<const EventModel> startEvent{};
    QPointer<const EventModel> endEvent{};
    QPointer<const TimeNodeModel> startNode{};
    QPointer<const TimeNodeModel> endNode{};
};


class DisplayedElementsProvider : public iscore::FactoryInterfaceBase
{
    public:
        virtual bool matches(const ConstraintModel& cst) const = 0;
        virtual DisplayedElementsContainer make(const ConstraintModel& cst) const = 0;
};

class ScenarioDisplayedElementsProvider : public DisplayedElementsProvider
{
    public:
        bool matches(const ConstraintModel& cst) const override
        {
            return dynamic_cast<Scenario::ScenarioModel*>(cst.parentScenario());
        }

        virtual DisplayedElementsContainer make(const ConstraintModel& cst) const override
        {
            if(auto parent_scenario = dynamic_cast<Scenario::ScenarioModel*>(cst.parentScenario()))
            {
                auto sst = &parent_scenario->states.at(cst.startState());
                auto est = &parent_scenario->states.at(cst.endState());
                auto sev = &parent_scenario->events.at(sst->eventId());
                auto eev = &parent_scenario->events.at(est->eventId());
                return {
                    &cst, sst, est, sev, eev,
                    &parent_scenario->timeNodes.at(sev->timeNode()),
                    &parent_scenario->timeNodes.at(eev->timeNode())
                };

            }

            return {};
        }

};

void DisplayedElementsModel::setSelection(const Selection & s)
{
    for_each_in_tuple(elements(), [&] (auto elt) {
        elt->selection.set(s.contains(elt.data())); // OPTIMIZEME
    });
 }

void DisplayedElementsModel::setDisplayedConstraint(const ConstraintModel& constraint)
{
    m_constraint = &constraint;
    if(auto parent_base = dynamic_cast<BaseScenario*>(m_constraint->parent()))
    {
        m_startNode = &parent_base->startTimeNode();
        m_endNode = &parent_base->endTimeNode();

        m_startEvent = &parent_base->startEvent();
        m_endEvent = &parent_base->endEvent();

        m_startState = &parent_base->startState();
        m_endState = &parent_base->endState();
    }
    else if(auto parent_scenario = dynamic_cast<Scenario::ScenarioModel*>(m_constraint->parent()))
    {
        m_startState = &parent_scenario->states.at(m_constraint->startState());
        m_endState = &parent_scenario->states.at(m_constraint->endState());

        m_startEvent = &parent_scenario->events.at(m_startState->eventId());
        m_endEvent = &parent_scenario->events.at(m_endState->eventId());

        m_startNode = &parent_scenario->timeNodes.at(m_startEvent->timeNode());
        m_endNode = &parent_scenario->timeNodes.at(m_endEvent->timeNode());
    }
}

const ConstraintModel &DisplayedElementsModel::constraint() const
{
    return *m_constraint;
}

const TimeNodeModel &DisplayedElementsModel::startTimeNode() const
{
    return *m_startNode;
}

const TimeNodeModel &DisplayedElementsModel::endTimeNode() const
{
    return *m_endNode;
}

const EventModel &DisplayedElementsModel::startEvent() const
{
    return *m_startEvent;
}

const EventModel &DisplayedElementsModel::endEvent() const
{
    return *m_endEvent;
}

const StateModel &DisplayedElementsModel::startState() const
{
    return *m_startState;
}

const StateModel &DisplayedElementsModel::endState() const
{
    return *m_endState;
}

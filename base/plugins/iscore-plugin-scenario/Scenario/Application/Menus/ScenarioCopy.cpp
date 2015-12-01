#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <boost/optional/optional.hpp>
#include <QJsonArray>
#include <QJsonValue>
#include <QList>
#include <algorithm>
#include <vector>

#include "ScenarioCopy.hpp"
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <core/document/DocumentContext.hpp>

class JSONObject;

template<typename Selected_T>
static auto arrayToJson(Selected_T &&selected)
{
    QJsonArray array;
    if (!selected.empty())
    {
        for (const auto &element : selected)
        {
            array.push_back(marshall<JSONObject>(*element));
        }
    }

    return array;
}

QJsonObject copySelectedScenarioElements(const Scenario::ScenarioModel& sm)
{
    auto selectedConstraints = selectedElements(sm.constraints);
    auto selectedEvents = selectedElements(sm.events);
    auto selectedTimeNodes = selectedElements(sm.timeNodes);
    auto selectedStates = selectedElements(sm.states);

    for(const ConstraintModel* constraint : selectedConstraints)
    {
        auto start_it = std::find_if(selectedStates.begin(), selectedStates.end(), [&] (const StateModel* state) { return state->id() == constraint->startState();});
        if(start_it == selectedStates.end())
        {
            selectedStates.push_back(&sm.states.at(constraint->startState()));
        }

        auto end_it = std::find_if(selectedStates.begin(), selectedStates.end(), [&] (const StateModel* state) { return state->id() == constraint->endState();});
        if(end_it == selectedStates.end())
        {
            selectedStates.push_back(&sm.states.at(constraint->endState()));
        }
    }

    for(const StateModel* state : selectedStates)
    {
        auto ev_it = std::find_if(selectedEvents.begin(), selectedEvents.end(), [&] (const EventModel* event) { return state->eventId() == event->id(); });
        if(ev_it == selectedEvents.end())
        {
            selectedEvents.push_back(&sm.events.at(state->eventId()));
        }

        // If the previous or next constraint is not here, we set it to null in a copy.
    }
    for(const EventModel* event : selectedEvents)
    {
        auto tn_it = std::find_if(selectedTimeNodes.begin(), selectedTimeNodes.end(), [&] (const TimeNodeModel* tn) { return tn->id() == event->timeNode(); });
        if(tn_it == selectedTimeNodes.end())
        {
            selectedTimeNodes.push_back(&sm.timeNodes.at(event->timeNode()));
        }

        // If some events aren't there, we set them to null in a copy.
    }

    std::vector<TimeNodeModel*> copiedTimeNodes;
    copiedTimeNodes.reserve(selectedTimeNodes.size());
    for(const auto& tn : selectedTimeNodes)
    {
        auto clone_tn = new TimeNodeModel(*tn, tn->id(), sm.parent());
        auto events = clone_tn->events();
        for(const auto& event : events)
        {
            auto absent = std::none_of(selectedEvents.begin(), selectedEvents.end(), [&] (const EventModel* ev) { return ev->id() == event; });
            if(absent)
                clone_tn->removeEvent(event);
        }

        copiedTimeNodes.push_back(clone_tn);
    }


    std::vector<EventModel*> copiedEvents;
    copiedEvents.reserve(selectedEvents.size());
    for(const auto& ev : selectedEvents)
    {
        auto clone_ev = new EventModel(*ev, ev->id(), sm.parent());
        auto states = clone_ev->states();
        for(const auto& state : states)
        {
            auto absent = std::none_of(selectedStates.begin(), selectedStates.end(), [&] (const StateModel* st) { return st->id() == state; });
            if(absent)
                clone_ev->removeState(state);
        }

        copiedEvents.push_back(clone_ev);
    }

    std::vector<StateModel*> copiedStates;
    copiedStates.reserve(selectedStates.size());
    auto& stack = iscore::IDocument::documentContext(sm).commandStack;
    for(const auto& st : selectedStates)
    {
        auto clone_st = new StateModel(*st, st->id(), stack, sm.parent());
        auto prev_absent = std::none_of(selectedConstraints.begin(), selectedConstraints.end(), [&] (const ConstraintModel* cst) { return cst->id() == st->previousConstraint(); });
        if(prev_absent)
            clone_st->setPreviousConstraint(Id<ConstraintModel>{});
        auto next_absent = std::none_of(selectedConstraints.begin(), selectedConstraints.end(), [&] (const ConstraintModel* cst) { return cst->id() == st->nextConstraint(); });
        if(next_absent)
            clone_st->setNextConstraint(Id<ConstraintModel>{});

        copiedStates.push_back(clone_st);
    }


    QJsonObject base;
    base["Constraints"] = arrayToJson(selectedConstraints);
    base["Events"] = arrayToJson(copiedEvents);
    base["TimeNodes"] = arrayToJson(copiedTimeNodes);
    base["States"] = arrayToJson(copiedStates);

    for(auto elt : copiedTimeNodes)
        delete elt;
    for(auto elt : copiedEvents)
        delete elt;
    for(auto elt : copiedStates)
        delete elt;

    return base;
}

QJsonObject copyBaseConstraint(const ConstraintModel& cst)
{
    QJsonObject base;
    QJsonArray arr;
    arr.push_back(marshall<JSONObject>(cst));
    base["Constraints"] = arr;

    return base;
}

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/Clamp.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <QHash>
#include <QJsonArray>
#include <QJsonValue>
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <vector>

#include <iscore/document/DocumentContext.hpp>
#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/AbstractScenarioLayerModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include "ScenarioPasteElements.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/std/Algorithms.hpp>
#include <core/document/Document.hpp>
// Needed for copy since we want to generate IDs that are neither
// in the scenario in which we are copying into, nor in the elements
// that we copied because it may cause conflicts.
// MOVEME
template<typename T, typename Vector1, typename Vector2>
static auto getStrongIdRange2(std::size_t s, const Vector1& existing1, const Vector2& existing2)
{
    std::vector<Id<T>> vec;
    vec.reserve(s + existing1.size() + existing2.size());
    std::transform(existing1.begin(), existing1.end(), std::back_inserter(vec),
                   [] (const auto& elt) { return elt.id(); });
    std::transform(existing2.begin(), existing2.end(), std::back_inserter(vec),
                   [] (const auto& elt) { return elt->id(); });

    for(; s --> 0 ;)
    {
        vec.push_back(getStrongId(vec));
    }
    auto final = std::vector<Id<T>>(vec.begin() + existing1.size() + existing2.size(), vec.end());

    return final;
}

template<typename T, typename Vector>
auto getStrongIdRangePtr(std::size_t s, const Vector& existing)
{
    std::vector<Id<T>> vec;
    vec.reserve(s + existing.size());
    std::transform(existing.begin(), existing.end(), std::back_inserter(vec),
                   [] (const auto& elt) { return elt->id(); });

    for(; s --> 0 ;)
    {
        vec.push_back(getStrongId(vec));
    }

    return std::vector<Id<T>>(vec.begin() + existing.size(), vec.end());
}

namespace Scenario
{
namespace Command
{
ScenarioPasteElements::ScenarioPasteElements(
        Path<TemporalScenarioLayerModel>&& path,
        const QJsonObject& obj,
        const Scenario::Point& pt):
    m_ts{std::move(path)}
{
    // We assign new ids WRT the elements of the scenario - these ids can
    // be easily mapped.
    const auto& tsModel = m_ts.find();
    const Scenario::ScenarioModel& scenario = ::model(tsModel);

    auto& doc = iscore::IDocument::documentContext(scenario);
    auto& stack = iscore::IDocument::documentContext(scenario).commandStack;


    std::vector<TimeNodeModel*> timenodes;
    std::vector<ConstraintModel*> constraints;
    std::vector<EventModel*> events;
    std::vector<StateModel*> states;

    // We deserialize everything
    {
        auto json_arr = obj["Constraints"].toArray();
        constraints.reserve(json_arr.size());
        for(const auto& element : json_arr)
        {
            constraints.emplace_back(new ConstraintModel{Deserializer<JSONObject>{element.toObject()}, &doc.document});
        }
    }
    {
        auto json_arr = obj["TimeNodes"].toArray();
        timenodes.reserve(json_arr.size());
        for(const auto& element : json_arr)
        {
            timenodes.emplace_back(new TimeNodeModel{Deserializer<JSONObject>{element.toObject()}, nullptr});
        }
    }
    {
        auto json_arr = obj["Events"].toArray();
        events.reserve(json_arr.size());
        for(const auto& element : json_arr)
        {
            events.emplace_back(new EventModel{Deserializer<JSONObject>{element.toObject()}, nullptr});
        }
    }
    {
        auto json_arr = obj["States"].toArray();
        states.reserve(json_arr.size());
        for(const auto& element : json_arr)
        {
            states.emplace_back(new StateModel{Deserializer<JSONObject>{element.toObject()}, stack, nullptr});
        }
    }

    // We generate identifiers for the forthcoming elements
    auto constraint_ids = getStrongIdRange2<ConstraintModel>(constraints.size(), scenario.constraints, constraints);
    auto timenode_ids = getStrongIdRange2<TimeNodeModel>(timenodes.size(), scenario.timeNodes, timenodes);
    auto event_ids = getStrongIdRange2<EventModel>(events.size(), scenario.events, events);
    auto state_ids = getStrongIdRange2<StateModel>(states.size(), scenario.states, states);

    // We set the new ids everywhere
    {
        int i = 0;
        for(TimeNodeModel* timenode : timenodes)
        {
            for(EventModel* event : events)
            {
                if(event->timeNode() == timenode->id())
                {
                    event->changeTimeNode(timenode_ids[i]);
                }
            }

            timenode->setId(timenode_ids[i]);
            i++;
        }
    }

    {
        int i = 0;
        for(EventModel* event : events)
        {
            {
                auto it = std::find_if(timenodes.begin(),
                                       timenodes.end(),
                                       [&] (TimeNodeModel* tn) { return tn->id() == event->timeNode(); });
                ISCORE_ASSERT(it != timenodes.end());
                auto timenode = *it;
                timenode->removeEvent(event->id());
                timenode->addEvent(event_ids[i]);
            }

            for(StateModel* state : states)
            {
                if(state->eventId() == event->id())
                {
                    state->setEventId(event_ids[i]);
                }
            }

            event->setId(event_ids[i]);
            i++;
        }
    }

    {
        int i = 0;
        for(StateModel* state : states)
        {
            {
                auto it = std::find_if(events.begin(),
                                       events.end(),
                                       [&] (EventModel* event) { return event->id() == state->eventId(); });
                ISCORE_ASSERT(it != events.end());
                auto event = *it;
                event->removeState(state->id());
                event->addState(state_ids[i]);
            }

            for(ConstraintModel* constraint : constraints)
            {
                if(constraint->startState() == state->id())
                    constraint->setStartState(state_ids[i]);
                else if(constraint->endState() == state->id())
                    constraint->setEndState(state_ids[i]);
            }

            state->setId(state_ids[i]);
            i++;
        }
    }

    {
        int i = 0;
        for(ConstraintModel* constraint : constraints)
        {
            constraint->setId(constraint_ids[i]);
            {
                auto start_state_id = find_if(states, [&] (auto state) {
                    return state->id() == constraint->startState();
                });
                if(start_state_id != states.end())
                    SetNextConstraint(**start_state_id, *constraint);
            }
            {
                auto end_state_id = find_if(states, [&] (auto state) {
                    return state->id() == constraint->endState();
                });
                if(end_state_id != states.end())
                    SetPreviousConstraint(**end_state_id, *constraint);
            }
            i++;
        }
    }


    // Then we have to create default constraint views... everywhere...
    for(const ConstraintModel* constraint : constraints)
    {
        m_constraintViewModels.insert(constraint->id(), ConstraintViewModelIdMap{});
    }
    for(const auto& viewModel : layers(scenario))
    {
        // We generate size(constraints) new view model ids.
        auto viewmodelconstraints = viewModel->constraints();

        int i = 0;
        auto range = getStrongIdRangePtr<ConstraintViewModel>(constraints.size(), viewmodelconstraints);
        for(const ConstraintModel* constraint : constraints)
        {
            auto& res = m_constraintViewModels[constraint->id()];
            res[*viewModel] = range[i++];
        }
        // We generate as many viewModel id as there are constraints to be added.

    }

    // Set the correct positions / dates.
    // Take the earliest constraint or timenode and compute the delta; apply the delta everywhere.
    if(constraints.size() > 0 || timenodes.size() > 0) // Should always be the case.
    {
        auto earliestTime =
                constraints.size() > 0
                ? constraints.front()->startDate()
                : timenodes.front()->date();
        for(const ConstraintModel* constraint : constraints)
        {
            const auto& t = constraint->startDate();
            if(t < earliestTime)
                earliestTime = t;
        }
        for(const TimeNodeModel* tn : timenodes)
        {
            const auto& t = tn->date();
            if(t < earliestTime)
                earliestTime = t;
        }
        for(const EventModel* ev : events)
        {
            const auto& t = ev->date();
            if(t < earliestTime)
                earliestTime = t;
        }

        auto delta_t = pt.date - earliestTime;
        for(ConstraintModel* constraint : constraints)
        {
            constraint->setStartDate(constraint->startDate() + delta_t);
        }
        for(TimeNodeModel* tn : timenodes)
        {
            tn->setDate(tn->date() + delta_t);
        }
        for(EventModel* ev : events)
        {
            ev->setDate(ev->date() + delta_t);
        }
    }

    // Same for y.
    // Note : due to the coordinates system, the highest y is the one closest to 0.
    auto highest_y = std::numeric_limits<double>::max();
    for(const StateModel* state : states)
    {
        auto y = state->heightPercentage();
        if(y < highest_y)
        {
            highest_y = y;
        }
    }

    auto delta_y = pt.y - highest_y;
    for(ConstraintModel* cst: constraints)
    {
        cst->setHeightPercentage(clamp(cst->heightPercentage() + delta_y, 0., 1.));
    }
    for(StateModel* state : states)
    {
        state->setHeightPercentage(clamp(state->heightPercentage() + delta_y, 0., 1.));
    }

    // We reserialize here in order to not have dangling pointers and bad cache in the ids.
    m_ids_constraints.reserve(constraints.size());
    m_json_constraints.reserve(constraints.size());
    for(auto elt : constraints)
    {
        m_ids_constraints.push_back(elt->id());
        m_json_constraints.push_back(marshall<JSONObject>(*elt));

        delete elt;
    }

    m_ids_timenodes.reserve(timenodes.size());
    m_json_timenodes.reserve(timenodes.size());
    for(auto elt : timenodes)
    {
        m_ids_timenodes.push_back(elt->id());
        m_json_timenodes.push_back(marshall<JSONObject>(*elt));

        delete elt;
    }

    m_json_events.reserve(events.size());
    m_ids_events.reserve(events.size());
    for(auto elt : events)
    {
        m_ids_events.push_back(elt->id());
        m_json_events.push_back(marshall<JSONObject>(*elt));

        delete elt;
    }

    m_json_states.reserve(states.size());
    m_ids_states.reserve(states.size());
    for(auto elt : states)
    {
        m_ids_states.push_back(elt->id());
        m_json_states.push_back(marshall<JSONObject>(*elt));

        delete elt;
    }
}

void ScenarioPasteElements::undo() const
{
    auto& tsModel = m_ts.find();
    Scenario::ScenarioModel& scenario = ::model(tsModel);

    for(const auto& elt : m_ids_timenodes)
    {
        scenario.timeNodes.remove(elt);
    }
    for(const auto& elt : m_ids_events)
    {
        scenario.events.remove(elt);
    }
    for(const auto& elt : m_ids_states)
    {
        scenario.states.remove(elt);
    }
    for(const auto& elt : m_ids_constraints)
    {
        scenario.constraints.remove(elt);
    }
}

void ScenarioPasteElements::redo() const
{
    auto& tsModel = m_ts.find();
    Scenario::ScenarioModel& scenario = ::model(tsModel);

    std::vector<TimeNodeModel*> addedTimeNodes;
    addedTimeNodes.reserve(m_json_timenodes.size());
    std::vector<EventModel*> addedEvents;
    addedEvents.reserve(m_json_events.size());
    for(const auto& timenode : m_json_timenodes)
    {
        auto tn = new TimeNodeModel(Deserializer<JSONObject>{timenode}, &scenario);
        scenario.timeNodes.add(tn);
        addedTimeNodes.push_back(tn);
    }

    for(const auto& event : m_json_events)
    {
        auto ev = new EventModel(Deserializer<JSONObject>{event}, &scenario);
        scenario.events.add(ev);
        addedEvents.push_back(ev);
    }

    auto& stack = iscore::IDocument::documentContext(scenario).commandStack;
    for(const auto& state : m_json_states)
    {
        scenario.states.add(new StateModel(Deserializer<JSONObject>{state}, stack, &scenario));
    }

    for(const auto& constraint : m_json_constraints)
    {
        auto cst = new ConstraintModel(Deserializer<JSONObject>{constraint}, &scenario);
        scenario.constraints.add(cst);

        createConstraintViewModels(m_constraintViewModels[cst->id()],
                                   cst->id(),
                                   scenario);

        if(cst->racks.size() > 0)
        {
            const auto& rackId = cst->racks.begin()->id();
            const auto& vms = cst->viewModels();
            for(ConstraintViewModel* vm : vms)
            {
                vm->showRack(rackId);
            }
        }
    }

    for(const auto& event : addedEvents)
    {
        updateEventExtent(event->id(), scenario);
    }
    for(const auto& timenode : addedTimeNodes)
    {
        updateTimeNodeExtent(timenode->id(), scenario);
    }
}

void ScenarioPasteElements::serializeImpl(DataStreamInput& s) const
{
    s << m_ts

      << m_ids_timenodes
      << m_ids_events
      << m_ids_states
      << m_ids_constraints

      << m_json_timenodes
      << m_json_events
      << m_json_states
      << m_json_constraints

      << m_constraintViewModels;
}

void ScenarioPasteElements::deserializeImpl(DataStreamOutput& s)
{
    s >> m_ts

      >> m_ids_timenodes
      >> m_ids_events
      >> m_ids_states
      >> m_ids_constraints

      >> m_json_timenodes
      >> m_json_events
      >> m_json_states
      >> m_json_constraints

      >> m_constraintViewModels;
}

}
}

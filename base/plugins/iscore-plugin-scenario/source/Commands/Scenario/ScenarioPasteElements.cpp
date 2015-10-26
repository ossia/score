#include "ScenarioPasteElements.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/State/StateModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>

#include <Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
#include <Process/ScenarioModel.hpp>
#include <Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <core/document/Document.hpp>

#include <iscore/tools/Clamp.hpp>

// Needed for copy since we want to generate IDs that are neither
// in the scenario in which we are copying into, nor in the elements
// that we copied because it may cause conflicts.
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

    return std::vector<Id<T>>(vec.begin() + existing1.size() + existing2.size(), vec.end());
}

ScenarioPasteElements::ScenarioPasteElements(
        Path<TemporalScenarioLayerModel>&& path,
        const QJsonObject& obj,
        const ScenarioPoint& pt):
    iscore::SerializableCommand{factoryName(), commandName(), description()},
    m_ts{std::move(path)}
{
    // We assign new ids WRT the elements of the scenario - these ids can
    // be easily mapped.
    const auto& tsModel = m_ts.find();
    const ScenarioModel& scenario = ::model(tsModel);

    // TODO the elements are child of the document
    // because else the State cannot be constructed properly
    // (it calls iscore::IDocument::commandStack...). This is ugly.
    auto doc = iscore::IDocument::documentFromObject(scenario);

    // We deserialize everything
    {
        auto json_arr = obj["Constraints"].toArray();
        m_constraints.reserve(json_arr.size());
        for(const auto& element : json_arr)
        {
            m_constraints.emplace_back(new ConstraintModel{Deserializer<JSONObject>{element.toObject()}, doc});
        }
    }
    {
        auto json_arr = obj["TimeNodes"].toArray();
        m_timenodes.reserve(json_arr.size());
        for(const auto& element : json_arr)
        {
            m_timenodes.emplace_back(new TimeNodeModel{Deserializer<JSONObject>{element.toObject()}, doc});
        }
    }
    {
        auto json_arr = obj["Events"].toArray();
        m_events.reserve(json_arr.size());
        for(const auto& element : json_arr)
        {
            m_events.emplace_back(new EventModel{Deserializer<JSONObject>{element.toObject()}, doc});
        }
    }
    {
        auto json_arr = obj["States"].toArray();
        m_states.reserve(json_arr.size());
        for(const auto& element : json_arr)
        {
            m_states.emplace_back(new StateModel{Deserializer<JSONObject>{element.toObject()}, doc});
        }
    }

    // We generate identifiers for the forthcoming elements
    auto constraint_ids = getStrongIdRange2<ConstraintModel>(m_constraints.size(), scenario.constraints, m_constraints);
    auto timenode_ids = getStrongIdRange2<TimeNodeModel>(m_timenodes.size(), scenario.timeNodes, m_timenodes);
    auto event_ids = getStrongIdRange2<EventModel>(m_events.size(), scenario.events, m_events);
    auto state_ids = getStrongIdRange2<StateModel>(m_states.size(), scenario.states, m_states);

    // We set the new ids everywhere
    {
        int i = 0;
        for(TimeNodeModel* timenode : m_timenodes)
        {
            for(EventModel* event : m_events)
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
        for(EventModel* event : m_events)
        {
            {
                auto it = std::find_if(m_timenodes.begin(),
                                       m_timenodes.end(),
                                       [&] (TimeNodeModel* tn) { return tn->id() == event->timeNode(); });
                ISCORE_ASSERT(it != m_timenodes.end());
                auto timenode = *it;
                timenode->removeEvent(event->id());
                timenode->addEvent(event_ids[i]);
            }

            for(StateModel* state : m_states)
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
        for(StateModel* state : m_states)
        {
            {
                auto it = std::find_if(m_events.begin(),
                                       m_events.end(),
                                       [&] (EventModel* event) { return event->id() == state->eventId(); });
                ISCORE_ASSERT(it != m_events.end());
                auto event = *it;
                event->removeState(state->id());
                event->addState(state_ids[i]);
            }

            for(ConstraintModel* constraint : m_constraints)
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
        for(ConstraintModel* constraint : m_constraints)
        {
            for(StateModel* state : m_states)
            {
                if(state->id() == constraint->startState())
                {
                    state->setNextConstraint(constraint_ids[i]);
                }
                else if(state->id() == constraint->endState())
                {
                    state->setPreviousConstraint(constraint_ids[i]);
                }
            }

            constraint->setId(constraint_ids[i]);
            i++;
        }
    }


    // Then we have to create default constraint views... everywhere...
    for(ConstraintModel* constraint : m_constraints)
    {
        auto res = m_constraintViewModels.insert(std::make_pair(constraint->id(), ConstraintViewModelIdMap{}));
        ISCORE_ASSERT(res.second);

        for(const auto& viewModel : layers(scenario))
        {
            res.first->second[*viewModel] = getStrongId(viewModel->constraints());
        }
    }

    // Set the correct positions / dates.
    // Take the earliest constraint or timenode and compute the delta; apply the delta everywhere.
    if(m_constraints.size() > 0 || m_timenodes.size() > 0) // Should always be the case.
    {
        auto earliestTime =
                m_constraints.size() > 0
                ? m_constraints.front()->startDate()
                : m_timenodes.front()->date();
        for(const ConstraintModel* constraint : m_constraints)
        {
            const auto& t = constraint->startDate();
            if(t < earliestTime)
                earliestTime = t;
        }
        for(const TimeNodeModel* tn : m_timenodes)
        {
            const auto& t = tn->date();
            if(t < earliestTime)
                earliestTime = t;
        }
        for(const EventModel* ev : m_events)
        {
            const auto& t = ev->date();
            if(t < earliestTime)
                earliestTime = t;
        }

        auto delta_t = pt.date - earliestTime;
        for(ConstraintModel* constraint : m_constraints)
        {
            constraint->setStartDate(constraint->startDate() + delta_t);
        }
        for(TimeNodeModel* tn : m_timenodes)
        {
            tn->setDate(tn->date() + delta_t);
        }
        for(EventModel* ev : m_events)
        {
            ev->setDate(ev->date() + delta_t);
        }
    }

    // Same for y.
    // Note : due to the coordinates system, the highest y is the one closest to 0.
    auto highest_y = std::numeric_limits<double>::max();
    for(const StateModel* state : m_states)
    {
        auto y = state->heightPercentage();
        if(y < highest_y)
        {
            highest_y = y;
        }
    }

    auto delta_y = pt.y - highest_y;
    for(ConstraintModel* cst: m_constraints)
    {
        cst->setHeightPercentage(clamp(cst->heightPercentage() + delta_y, 0., 1.));
    }
    for(StateModel* state : m_states)
    {
        state->setHeightPercentage(clamp(state->heightPercentage() + delta_y, 0., 1.));
    }
}

void ScenarioPasteElements::undo() const
{
    ISCORE_TODO;
}

#include <Process/Algorithms/VerticalMovePolicy.hpp>
void ScenarioPasteElements::redo() const
{
    auto& tsModel = m_ts.find();
    ScenarioModel& scenario = ::model(tsModel);

    std::vector<TimeNodeModel*> addedTimeNodes;
    addedTimeNodes.reserve(m_timenodes.size());
    std::vector<EventModel*> addedEvents;
    addedEvents.reserve(m_events.size());
    for(const auto& timenode : m_timenodes)
    {
        auto tn = new TimeNodeModel(*timenode, timenode->id(), &scenario);
        scenario.timeNodes.add(tn);
        addedTimeNodes.push_back(tn);
    }

    for(const auto& event : m_events)
    {
        auto ev = new EventModel(*event, event->id(), &scenario);
        scenario.events.add(ev);
        addedEvents.push_back(ev);
    }

    for(const auto& state : m_states)
    {
        scenario.states.add(new StateModel(*state, state->id(), &scenario));
    }

    for(const auto& constraint : m_constraints)
    {
        auto cst = new ConstraintModel(*constraint, constraint->id(), &scenario);
        scenario.constraints.add(cst);

        createConstraintViewModels(m_constraintViewModels.at(constraint->id()),
                                   constraint->id(),
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

void ScenarioPasteElements::serializeImpl(QDataStream&) const
{
    ISCORE_TODO;
}

void ScenarioPasteElements::deserializeImpl(QDataStream&)
{
    ISCORE_TODO;
}

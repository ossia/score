#include "RemoveSelection.hpp"

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <Scenario/Process/Algorithms/StandardRemovalPolicy.hpp>

using namespace iscore;
using namespace Scenario::Command;

RemoveSelection::RemoveSelection(Path<ScenarioModel>&& scenarioPath, Selection sel):
    SerializableCommand{factoryName(), commandName(), description()},
    m_path {std::move(scenarioPath) }
{
    auto& scenar = m_path.find();

    // Serialize all the events and constraints and timenodes and states.
    // For each removed event, we also add its states to the selection.
    // For each removed state, we add the constraints.

    // First we remove :
    // The event if a State is selected and alone

    // Then we have to make a round to remove all the events
    // of the selected time nodes.

    Selection cp = sel;
    for(const auto& obj : cp)
    {
        if(auto state = dynamic_cast<const StateModel*>(obj.data()))
        {
            auto& ev = scenar.events.at(state->eventId());
            if(ev.states().size() == 1)
            {
                sel.append(&ev);
            }
        }
    }

    cp = sel;
    for(const auto& obj : cp)
    {
        if(auto tn = dynamic_cast<const TimeNodeModel*>(obj.data()))
        {
            for (const auto& ev : tn->events())
            {
                sel.append(&scenar.events.at(ev));
            }
        }
    }


    QList<TimeNodeModel*> maybeRemovedTimenodes;
    cp = sel;
    for(const auto& obj : cp) // Make a copy
    {
        if(auto event = dynamic_cast<const EventModel*>(obj.data()))
        {
            // TODO have scenario take something that takes a container of ids
            // and return the corresponding elements.
            for(const auto& state : event->states())
            {
                sel.append(&scenar.states.at(state));
            }

            // This timenode may be removed if the event is alone.
            auto tn = &scenar.timeNodes.at(event->timeNode());
            if(!sel.contains(tn))
                maybeRemovedTimenodes.append(tn);
        }
    }

    cp = sel;
    for(const auto& obj : cp)
    {
        if(auto state = dynamic_cast<const StateModel*>(obj.data()))
        {
            if(state->previousConstraint())
                sel.append(&scenar.constraints.at(state->previousConstraint()));
            if(state->nextConstraint())
                sel.append(&scenar.constraints.at(state->nextConstraint()));
        }
    }

    // Serialize ALL the things
    for(const auto& obj : sel)
    {
        if(auto state = dynamic_cast<const StateModel*>(obj.data()))
        {
            QByteArray arr;
            Serializer<DataStream> s{&arr};
            s.readFrom(*state);
            m_removedStates.push_back({state->id(), arr});
        }

        if(auto event = dynamic_cast<const EventModel*>(obj.data()))
        {
            if(event->id() != Id<EventModel>{0})
            {
                QByteArray arr;
                Serializer<DataStream> s{&arr};
                s.readFrom(*event);
                m_removedEvents.push_back({event->id(), arr});
            }
        }

        if(auto tn = dynamic_cast<const TimeNodeModel*>(obj.data()))
        {
            if(tn->id() != Id<TimeNodeModel>{0})
            {
                QByteArray arr;
                Serializer<DataStream> s2{&arr};
                s2.readFrom(*tn);
                m_removedTimeNodes.push_back({tn->id(), arr});
            }
        }

        if(auto constraint = dynamic_cast<const ConstraintModel*>(obj.data()))
        {
            QByteArray arr;
            Serializer<DataStream> s{&arr};
            s.readFrom(*constraint);
            m_removedConstraints.push_back(
            {
                {constraint->id(), arr},
                serializeConstraintViewModels(*constraint, scenar)
            });
        }
    }

    // Plus the timenodes that we don't know if they will be removed (todo ugly fixme pls)
    // TODO how does this even work ? what happens of the maybe removed events / states ?
    for(const auto& tn : maybeRemovedTimenodes)
    {
        if(tn->id() != Id<TimeNodeModel>{0})
        {
            QByteArray arr;
            Serializer<DataStream> s2{&arr};
            s2.readFrom(*tn);
            m_maybeRemovedTimeNodes.push_back({tn->id(), arr});
        }
    }
}

void RemoveSelection::undo() const
{
    auto& scenar = m_path.find();
    // First instantiate everything

    QList<StateModel*> states;
    std::transform(m_removedStates.begin(),
                   m_removedStates.end(),
                   std::back_inserter(states),
                   [&] (const auto& data)
    {
        Deserializer<DataStream> s{data.second};
        return new StateModel{s, &scenar};
    });

    QList<EventModel*> events;
    std::transform(m_removedEvents.begin(),
                   m_removedEvents.end(),
                   std::back_inserter(events),
                   [&] (const auto& eventdata)
    {
        Deserializer<DataStream> s{eventdata.second};
        return new EventModel{s, &scenar};
    });

    QList<TimeNodeModel*> timenodes;
    std::transform(m_removedTimeNodes.begin(),
                   m_removedTimeNodes.end(),
                   std::back_inserter(timenodes),
                   [&] (const auto& tndata)
    {
        Deserializer<DataStream> s{tndata.second};
        return new TimeNodeModel{s, &scenar};
    });


    QList<TimeNodeModel*> maybeTimenodes;
    std::transform(m_maybeRemovedTimeNodes.begin(),
                   m_maybeRemovedTimeNodes.end(),
                   std::back_inserter(maybeTimenodes),
                   [&] (const auto& tndata)
    {
        Deserializer<DataStream> s{tndata.second};
        return new TimeNodeModel{s, &scenar};
    });

    // Recreate all the removed timenodes
    for(auto& tn : timenodes)
    {
        // The events should be removed first because else
        // signals may sent and the event may not be found...
        // They will be re-added anyway.
        auto events_in_timenode = tn->events();
        for(auto& event : events_in_timenode)
        {
            tn->removeEvent(event);
        }
        scenar.timeNodes.add(tn);
    }

    // Recreate first all the events / maybe removed timenodes
    for(auto& event : events)
    {
        // We have to make a copy at each iteration since each iteration
        // might add a timenode.
        auto timenodes_in_scenar = scenar.timeNodes.map();
        auto scenar_timenode_it = std::find(timenodes_in_scenar.begin(),
                                     timenodes_in_scenar.end(),
                                     event->timeNode());
        if(scenar_timenode_it != timenodes_in_scenar.end())
        {
            // The timenode already exists
            // Hence we don't need the one we serialized.
            auto to_delete = std::find(maybeTimenodes.begin(),
                                       maybeTimenodes.end(),
                                       event->timeNode());

            // TODO why do we need to check for this ? ISCORE_ASSERT sometime fails...
            if(to_delete != maybeTimenodes.end())
            {
                delete *to_delete;
                maybeTimenodes.erase(to_delete);
            }

            // We can add our event to the scenario.
            scenar.events.add(event);

            // Maybe this shall be done after everything has been added to prevent problems ?
            (*scenar_timenode_it).addEvent(event->id());
        }
        else
        {
            // We have to insert the timenode that was removed.
            auto removed_timenode_it = std::find(maybeTimenodes.begin(),
                                                 maybeTimenodes.end(),
                                                 event->timeNode());
            ISCORE_ASSERT(removed_timenode_it != maybeTimenodes.end());
            TimeNodeModel* timeNode = *removed_timenode_it;

            maybeTimenodes.erase(removed_timenode_it);

            // First, since the event is not yet in the scenario
            // we remove it from the timenode since it might crash
            timeNode->removeEvent(event->id());

            // And we add the timenode
            scenar.timeNodes.add(timeNode);

            // We can re-add the event.
            scenar.events.add(event);
            timeNode->addEvent(event->id());
        }
    }

    // All the states
    for(const auto& state : states)
    {
        scenar.states.add(state);
    }

    // And then all the constraints.
    for (const auto& constraintdata : m_removedConstraints)
    {
        Deserializer<DataStream> s{constraintdata.first.second};
        auto cstr = new ConstraintModel{s, &scenar};

        scenar.constraints.add(cstr);

        // Set-up the start / end events correctly.
        auto& sst = scenar.state(cstr->startState());
        sst.setNextConstraint(cstr->id());

        auto& est = scenar.state(cstr->endState());
        est.setPreviousConstraint(cstr->id());
    }

    // And finally the constraint's view models
    for(const auto& constraintdata : m_removedConstraints)
    {
        // view model creation
        deserializeConstraintViewModels(constraintdata.second, scenar);
    }
}

void RemoveSelection::redo() const
{
    auto& scenar = m_path.find();
    // Remove the constraints
    for(const auto& cstr : m_removedConstraints)
    {
        StandardRemovalPolicy::removeConstraint(scenar, cstr.first.first);
    }

    // The other things
    for(const auto& ev : m_removedEvents)
    {
        StandardRemovalPolicy::removeEventStatesAndConstraints(scenar, ev.first);
    }

    // Finally if there are remaining states
    for(const auto& st : m_removedStates)
    {
        auto it = scenar.states.find(st.first);
        if(it != scenar.states.end())
        {
            StandardRemovalPolicy::removeState(scenar, *it);
        }
    }
}

void RemoveSelection::serializeImpl(QDataStream& s) const
{
    s << m_path
      << m_maybeRemovedTimeNodes
      << m_removedEvents
      << m_removedTimeNodes
      << m_removedConstraints;
}

void RemoveSelection::deserializeImpl(QDataStream& s)
{
    s >> m_path
      >> m_maybeRemovedTimeNodes
      >> m_removedEvents
      >> m_removedTimeNodes
      >> m_removedConstraints;
}

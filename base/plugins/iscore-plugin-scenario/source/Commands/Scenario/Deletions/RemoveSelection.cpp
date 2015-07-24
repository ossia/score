#include "RemoveSelection.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Process/Temporal/TemporalScenarioLayerModel.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveSelection::RemoveSelection(ObjectPath&& scenarioPath, Selection sel):
    SerializableCommand{"ScenarioControl", commandName(), description()},
    m_path {std::move(scenarioPath) }
{
    auto& scenar = m_path.find<ScenarioModel>();

    // Serialize all the events and constraints and timenodes and states.
    // For each removed event, we also add its states to the selection.
    // For each removed state, we add the constraints.

    // First we have to make a first round to remove all the events
    // of the selected time nodes.

    for(const auto& obj : sel)
    {
        if(auto tn = dynamic_cast<const TimeNodeModel*>(obj))
        {
            for (const auto& ev : tn->events())
            {
                sel.insert(&scenar.event(ev));
            }
        }
    }


    QList<TimeNodeModel*> maybeRemovedTimenodes;
    for(const auto& obj : (sel)) // Make a copy
    {
        if(auto event = dynamic_cast<const EventModel*>(obj))
        {
            // TODO have scenario take something that takes a container of ids
            // and return the corresponding elements.
            for(const auto& state : event->states())
            {
                sel.insert(&scenar.state(state));
            }

            // This timenode may be removed if the event is alone.
            auto tn = &scenar.timeNode(event->timeNode());
            if(sel.find(tn) == sel.end())
                maybeRemovedTimenodes.append(tn);
        }
    }

    for(const auto& obj : (sel))
    {
        if(auto state = dynamic_cast<const StateModel*>(obj))
        {
            if(state->previousConstraint())
                sel.insert(&scenar.constraint(state->previousConstraint()));
            if(state->nextConstraint())
                sel.insert(&scenar.constraint(state->nextConstraint()));
        }
    }

    // Serialize ALL the things
    for(auto& obj : sel)
    {
        if(auto state = dynamic_cast<const StateModel*>(obj))
        {
            QByteArray arr;
            Serializer<DataStream> s{&arr};
            s.readFrom(*state);
            m_removedStates.push_back({state->id(), arr});
        }

        if(auto event = dynamic_cast<const EventModel*>(obj))
        {
            QByteArray arr;
            Serializer<DataStream> s{&arr};
            s.readFrom(*event);
            m_removedEvents.push_back({event->id(), arr});
        }

        if(auto tn = dynamic_cast<const TimeNodeModel*>(obj))
        {
            QByteArray arr;
            Serializer<DataStream> s2{&arr};
            s2.readFrom(*tn);
            m_removedTimeNodes.push_back({tn->id(), arr});
        }

        if(auto constraint = dynamic_cast<const ConstraintModel*>(obj))
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
    for(const auto& tn : maybeRemovedTimenodes)
    {
        QByteArray arr;
        Serializer<DataStream> s2{&arr};
        s2.readFrom(*tn);
        m_maybeRemovedTimeNodes.push_back({tn->id(), arr});
    }
}

void RemoveSelection::undo()
{
    auto& scenar = m_path.find<ScenarioModel>();
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
        scenar.addTimeNode(tn);
    }

    // Recreate first all the events / maybe removed timenodes
    for(auto& event : events)
    {
        // We have to make a copy at each iteration since each iteration
        // might add a timenode.
        auto timenodes_in_scenar = scenar.timeNodes();
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

            // TODO why do we need to check for this ? Q_ASSERT sometime fails...
            if(to_delete != maybeTimenodes.end())
            {
                delete *to_delete;
                maybeTimenodes.erase(to_delete);
            }

            // We can add our event to the scenario.
            scenar.addEvent(event);

            // Maybe this shall be done after everything has been added to prevent problems ?
            (*scenar_timenode_it).addEvent(event->id());
        }
        else
        {
            // We have to insert the timenode that was removed.
            auto removed_timenode_it = std::find(maybeTimenodes.begin(),
                                                 maybeTimenodes.end(),
                                                 event->timeNode());
            Q_ASSERT(removed_timenode_it != maybeTimenodes.end());
            TimeNodeModel* timeNode = *removed_timenode_it;

            maybeTimenodes.erase(removed_timenode_it);

            // First, since the event is not yet in the scenario
            // we remove it from the timenode since it might crash
            timeNode->removeEvent(event->id());

            // And we add the timenode
            scenar.addTimeNode(timeNode);

            // We can re-add the event.
            scenar.addEvent(event);
            timeNode->addEvent(event->id());
        }
    }

    // All the states
    for(const auto& state : states)
    {
        scenar.addState(state);
    }

    // And then all the constraints.
    for (const auto& constraintdata : m_removedConstraints)
    {
        Deserializer<DataStream> s{constraintdata.first.second};
        auto cstr = new ConstraintModel{s, &scenar};

        scenar.addConstraint(cstr);

        // Set-up the start / end events correctly.
        auto& sst = scenar.state(cstr->startState());
        if (sst.nextConstraint() != cstr->id())
            sst.setNextConstraint(cstr->id());

        auto& est = scenar.state(cstr->endState());
        if (est.previousConstraint() != cstr->id())
            est.setPreviousConstraint(cstr->id());
    }

    // And finally the constraint's view models
    for(const auto& constraintdata : m_removedConstraints)
    {
        // view model creation
        deserializeConstraintViewModels(constraintdata.second, scenar);
    }
}

void RemoveSelection::redo()
{
    auto& scenar = m_path.find<ScenarioModel>();
    // Remove the constraints
    for(auto& cstr : m_removedConstraints)
    {
        StandardRemovalPolicy::removeConstraint(scenar, cstr.first.first);
    }

    // The other things
    for(auto& ev : m_removedEvents)
    {
        StandardRemovalPolicy::removeEventStatesAndConstraints(scenar, ev.first);
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

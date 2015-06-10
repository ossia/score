#include "RemoveSelection.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveSelection::RemoveSelection(ObjectPath&& scenarioPath, Selection sel):
    SerializableCommand{"ScenarioControl", className(), description()},
    m_path {std::move(scenarioPath) }
{
    auto& scenar = m_path.find<ScenarioModel>();

    // Serialize all the events and constraints and timenodes.
    // For each removed event, we also add its constraints to the selection.
    Selection other;
    // First we have to make a first round to remove all the events
    // of the selected time nodes.

    for(const auto& obj : sel)
    {
        if(auto tn = dynamic_cast<const TimeNodeModel*>(obj))
        {
            for (const auto& ev : tn->events())
            {
                other.append(&scenar.event(ev));
            }
        }
    }

    // Remove duplicates
    sel = (sel + other).toSet().toList();

    QList<TimeNodeModel*> maybeRemovedTimenodes;
    for(const auto& obj : sel)
    {
        if(auto event = dynamic_cast<const EventModel*>(obj))
        {
            // TODO have scenario take something that takes a container of ids
            // and return the corresponding elements.
            for (const auto& cstr : event->constraints())
            {
                other.append(&scenar.constraint(cstr));
            }

            // This timenode may be removed if the event is alone.
            auto tn = &scenar.timeNode(event->timeNode());
            if(!sel.contains(tn))
                maybeRemovedTimenodes.append(tn);
        }
    }

    // Remove duplicates
    sel = (sel + other).toSet().toList();

    // Serialize ALL the things
    for(auto& obj : sel)
    {
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

    // Plus the timenodes that we don't know if they will be removed (ugly fixme pls)
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
            (*scenar_timenode_it)->addEvent(event->id());
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

            // TODO Why is it necessary to remove / readd?
            // Maybe we should do a first pass where we only add the
            // timenodes, and then all the events in bulk ?
            timeNode->removeEvent(event->id());
            scenar.addTimeNode(timeNode);

            scenar.addEvent(event);
            timeNode->addEvent(event->id());
        }
    }

    // And then all the constraints.
    for (const auto& constraintdata : m_removedConstraints)
    {
        Deserializer<DataStream> s{constraintdata.first.second};
        auto cstr = new ConstraintModel{s, &scenar};

        scenar.addConstraint(cstr);

        // Set-up the start / end events correctly.
        auto& sev = scenar.event(cstr->startEvent());
        if (!sev.nextConstraints().contains(cstr->id()))
            sev.addNextConstraint(cstr->id());

        auto& eev = scenar.event(cstr->endEvent());
        if (!eev.previousConstraints().contains(cstr->id()))
            eev.addPreviousConstraint(cstr->id());

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

    // Remove the events; the timenodes will be removed automatically.
    for(auto& ev : m_removedEvents)
    {
        StandardRemovalPolicy::removeEventAndConstraints(scenar, ev.first);
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

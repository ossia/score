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
    /*
    auto& scenar = m_path.find<ScenarioModel>();

    Deserializer<DataStream> s{m_serializedEvent};
    auto event = new EventModel{s, &scenar};

    Deserializer<DataStream> s2 {m_serializedTimeNode};
    auto timeNode = new TimeNodeModel{s2, &scenar};
    bool tnFound = false;

    for (auto tn : scenar.timeNodes())
    {
        if (tn->id() == event->timeNode())
        {
            delete timeNode;
            scenar.addEvent(event);
            tn->addEvent(event->id());
            tnFound = true;
            break;
        }
    }
    if (! tnFound)
    {
        timeNode->removeEvent(event->id());
        scenar.addTimeNode(timeNode);
        scenar.addEvent(event);
        timeNode->addEvent(event->id());
    }

    // re-create constraints
    for (auto scstr : m_removedConstraints)
    {
        Deserializer<DataStream> s{scstr.first};
        auto cstr = new ConstraintModel{s, &scenar};

        scenar.addConstraint(cstr);

        // adding constraint to second extremity event
        if (m_evId == cstr->endEvent())
            scenar.event(cstr->startEvent()).addNextConstraint(cstr->id());
        else
            scenar.event(cstr->endEvent()).addPreviousConstraint(cstr->id());

        // view model creation
        deserializeConstraintViewModels(scstr.second, scenar);
    }
    */
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

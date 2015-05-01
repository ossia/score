#include "RemoveEvent.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveEvent::RemoveEvent(const ObjectPath &scenarioPath, EventModel *event):
    RemoveEvent{ObjectPath{scenarioPath}, event}
{

}

RemoveEvent::RemoveEvent(ObjectPath&& scenarioPath, EventModel* event) :
    SerializableCommand{"ScenarioControl", className(), description()},
    m_path {std::move(scenarioPath) }
{
    QByteArray arr;
    Serializer<DataStream> s{&arr};
    s.readFrom(*event);
    m_serializedEvent = arr;

    m_evId = event->id();

    auto scenar = m_path.find<ScenarioModel>();
    auto timeNode = scenar->timeNode(event->timeNode());

    QByteArray arr2;
    Serializer<DataStream> s2{&arr2};
    s2.readFrom(*timeNode);
    m_serializedTimeNode = arr2;

    for (auto cstr : event->constraints())
    {
        ConstraintModel* constraint = scenar->constraint(cstr);

        QByteArray arr;
        Serializer<DataStream> s{&arr};
        s.readFrom(*constraint);
        m_serializedConstraints.push_back({arr, serializeConstraintViewModels(constraint, scenar)});
    }
}

void RemoveEvent::undo()
{
    auto scenar = m_path.find<ScenarioModel>();

    Deserializer<DataStream> s {&m_serializedEvent};
    auto event = new EventModel(s, scenar);

    Deserializer<DataStream> s2 {&m_serializedTimeNode};
    auto timeNode = new TimeNodeModel(s2, scenar);
    bool tnFound = false;

    for (auto tn : scenar->timeNodes())
    {
        if (tn->id() == event->timeNode())
        {
            delete timeNode;
            scenar->addEvent(event);
            tn->addEvent(event->id());
            tnFound = true;
            break;
        }
    }
    if (! tnFound)
    {
        timeNode->removeEvent(event->id());
        scenar->addTimeNode(timeNode);
        scenar->addEvent(event);
        timeNode->addEvent(event->id());
    }

    // re-create constraints
    for (auto scstr : m_serializedConstraints)
    {
        Deserializer<DataStream> s{&scstr.first};
        auto cstr = new ConstraintModel(s, scenar);

        scenar->addConstraint(cstr);

        // adding constraint to second extremity event
        if (m_evId == cstr->endEvent())
            scenar->event(cstr->startEvent())->addNextConstraint(cstr->id());
        else
            scenar->event(cstr->endEvent())->addPreviousConstraint(cstr->id());

        // view model creation
        deserializeConstraintViewModels(scstr.second, scenar);
    }
}

void RemoveEvent::redo()
{
    auto scenar = m_path.find<ScenarioModel>();
    StandardRemovalPolicy::removeEventAndConstraints(*scenar, m_evId);
}

void RemoveEvent::serializeImpl(QDataStream& s) const
{
    s << m_path << m_evId << m_serializedEvent << m_serializedConstraints << m_serializedTimeNode ;
}

void RemoveEvent::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_evId >> m_serializedEvent >> m_serializedConstraints >> m_serializedTimeNode ;
}

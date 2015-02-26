#include "RemoveEvent.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Event/State/State.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "source/ProcessInterfaceSerialization/ProcessSharedModelInterfaceSerialization.hpp"

#include <core/tools/utilsCPP11.hpp>

using namespace iscore;
using namespace Scenario::Command;

RemoveEvent::RemoveEvent() :
    SerializableCommand {"ScenarioControl",
    "RemoveEvent",
    QObject::tr("Remove event and pre-constraints")
}
{
}


RemoveEvent::RemoveEvent(ObjectPath&& scenarioPath, EventModel* event) :
    SerializableCommand {"ScenarioControl",
    "RemoveEvent",
    QObject::tr("Remove event and pre-constraints")
},
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

    /*
        for (auto cstr : event->previousConstraints())
        {
            QByteArray arr;
            Serializer<DataStream> s{&arr};
            s.readFrom(cstr);
            m_serializedConstraints.push_back(arr);
        }
        for (auto cstr : event->nextConstraints())
        {
            QByteArray arr;
            Serializer<DataStream> s{&arr};
            s.readFrom(cstr);
            m_serializedConstraints.push_back(arr);
        }
    */

}

void RemoveEvent::undo()
{

    auto scenar = m_path.find<ScenarioModel>();

    Deserializer<DataStream> s2 {&m_serializedTimeNode};
    auto timeNode = new TimeNodeModel(s2, scenar);

    Deserializer<DataStream> s {&m_serializedEvent};
    auto event = new EventModel(s, scenar);

    timeNode->removeEvent(event->id());
    event->changeTimeNode(id_type<TimeNodeModel> (0));

    scenar->addTimeNode(timeNode);
    scenar->addEvent(event);

    timeNode->addEvent(event->id());
    event->changeTimeNode(timeNode->id());


    // todo : recréer les contraintes. En attendant, on les supprimes de EventModel pour eviter les crashs.
    for(auto cstr : event->previousConstraints())
    {
        event->removePreviousConstraint(cstr);
    }

    for(auto cstr : event->nextConstraints())
    {
        event->removeNextConstraint(cstr);
    }

    /*
        for (auto scstr : m_serializedConstraints)
        {
            Deserializer<DataStream> s{&scstr};
            scenar->addConstraint(new ConstraintModel(s, scenar));
        }
    */
}

void RemoveEvent::redo()
{
    auto scenar = m_path.find<ScenarioModel>();
    scenar->removeEvent(m_evId);
}

int RemoveEvent::id() const
{
    return 1;
}

bool RemoveEvent::mergeWith(const QUndoCommand* other)
{
    return false;
}

void RemoveEvent::serializeImpl(QDataStream& s) const
{
    s << m_path << m_evId << m_serializedEvent << m_serializedConstraints << m_serializedTimeNode ;
}

void RemoveEvent::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_evId >> m_serializedEvent >> m_serializedConstraints >> m_serializedTimeNode ;
}

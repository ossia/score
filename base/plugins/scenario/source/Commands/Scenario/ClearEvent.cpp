#include "ClearEvent.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/State/State.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioModel.hpp"

#include <public_interface/tools/utilsCPP11.hpp>

using namespace iscore;
using namespace Scenario::Command;

ClearEvent::ClearEvent(ObjectPath&& eventPath) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_path {std::move(eventPath) }
{

    auto event = m_path.find<EventModel>();

    for(const State* state : event->states())
    {
        QByteArray arr;
        Serializer<DataStream> s {&arr};
        s.readFrom(*state);
        m_serializedStates.push_back(arr);
    }
}

void ClearEvent::undo()
{
    auto event = m_path.find<EventModel>();

    for(auto& serializedState : m_serializedStates)
    {
        Deserializer<DataStream> s {&serializedState};
        event->addState(new FakeState {s, event});
    }
}

void ClearEvent::redo()
{
    auto event = m_path.find<EventModel>();

    for(auto& state : event->states())
    {
        event->removeState(state->id());
    }

}

bool ClearEvent::mergeWith(const Command* other)
{
    return false;
}

void ClearEvent::serializeImpl(QDataStream& s) const
{
    s << m_path << m_serializedStates;
}

void ClearEvent::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_serializedStates;
}

#include "RemoveStateFromEvent.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/State/State.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveStateFromEvent::RemoveStateFromEvent(ObjectPath &&eventPath, QString message):
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_path {std::move(eventPath) },
m_message(message)
{
    auto event = m_path.find<EventModel>();
    for (State* state : event->states())
    {
        for (QString msg : state->messages())
        {
            if (msg == m_message)
            {
                m_stateId = state->id();

                QByteArray arr;
                Serializer<DataStream> s{&arr};
                s.readFrom(*state);
                m_serializedState = arr;

                break;
            }
        }
    }
}

void RemoveStateFromEvent::undo()
{
    auto event = m_path.find<EventModel>();
    Deserializer<DataStream> s{&m_serializedState};
    FakeState* state = new FakeState{s, event};
    event->addState(state);
}

void RemoveStateFromEvent::redo()
{
    auto event = m_path.find<EventModel>();
    event->removeState(m_stateId);
}

bool RemoveStateFromEvent::mergeWith(const Command* other)
{
    return false;
}

void RemoveStateFromEvent::serializeImpl(QDataStream& s) const
{
    s << m_path << m_message << m_stateId;
}

void RemoveStateFromEvent::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_message >> m_stateId;
}

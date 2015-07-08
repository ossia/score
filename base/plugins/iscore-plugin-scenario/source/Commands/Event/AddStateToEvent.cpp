#include "AddStateToEvent.hpp"

#include "Document/Event/EventModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

AddStateToEvent::AddStateToEvent(ObjectPath&& eventPath, const iscore::State &state) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {std::move(eventPath) },
    m_state{state}
{

}

AddStateToEvent::AddStateToEvent(ObjectPath&& eventPath, iscore::State &&state) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {std::move(eventPath)},
    m_state{std::move(state)}
{

}

void AddStateToEvent::undo()
{
    /*
    auto& event = m_path.find<EventModel>();
    event.removeState(m_state);
    */
}

void AddStateToEvent::redo()
{
    /*
    auto& event = m_path.find<EventModel>();
    event.addState(m_state);
    */
}

void AddStateToEvent::serializeImpl(QDataStream& s) const
{
    /*
    s << m_path << m_state;
    */
}

void AddStateToEvent::deserializeImpl(QDataStream& s)
{
    /*
    s >> m_path >> m_state;
    */
}

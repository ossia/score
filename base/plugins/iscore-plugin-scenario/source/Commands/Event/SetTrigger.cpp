#include "SetTrigger.hpp"

#include "Document/Event/EventModel.hpp"

using namespace iscore;
using namespace Scenario::Command;


SetTrigger::SetTrigger(ObjectPath&& eventPath, QString message) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_path {std::move(eventPath) },
m_trigger(message)
{
    auto& event = m_path.find<EventModel>();
    m_previousTrigger = event.trigger();
}

void SetTrigger::undo()
{
    auto& event = m_path.find<EventModel>();
    event.setTrigger(m_previousTrigger);
}

void SetTrigger::redo()
{
    auto& event = m_path.find<EventModel>();
    event.setTrigger(m_trigger);
}

void SetTrigger::serializeImpl(QDataStream& s) const
{
    s << m_path << m_trigger << m_previousTrigger;
}

void SetTrigger::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_trigger >> m_previousTrigger;
}

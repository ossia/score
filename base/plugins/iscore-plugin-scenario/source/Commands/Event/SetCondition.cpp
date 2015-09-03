#include "SetCondition.hpp"

#include "Document/Event/EventModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

// TODO
SetCondition::SetCondition(
        Path<EventModel>&& eventPath,
        iscore::Condition&& cond) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {std::move(eventPath) },
    m_condition(std::move(cond))
{

    auto& event = m_path.find();
    m_previousCondition = event.condition();
}

void SetCondition::undo()
{
    auto& event = m_path.find();
    event.setCondition(m_previousCondition);
}

void SetCondition::redo()
{
    auto& event = m_path.find();
    event.setCondition(m_condition);
}

void SetCondition::serializeImpl(QDataStream& s) const
{
    ISCORE_TODO;
   // s << m_path << m_condition << m_previousCondition;
}

void SetCondition::deserializeImpl(QDataStream& s)
{
    ISCORE_TODO;
    //s >> m_path >> m_condition >> m_previousCondition;
}

#include "SetCondition.hpp"

#include <Scenario/Document/Event/EventModel.hpp>

using namespace iscore;
using namespace Scenario::Command;

SetCondition::SetCondition(
        Path<EventModel>&& eventPath,
        iscore::Condition&& cond) :
    m_path {std::move(eventPath) },
    m_condition(std::move(cond))
{

    auto& event = m_path.find();
    m_previousCondition = event.condition();
}

void SetCondition::undo() const
{
    auto& event = m_path.find();
    event.setCondition(m_previousCondition);
}

void SetCondition::redo() const
{
    auto& event = m_path.find();
    event.setCondition(m_condition);
}

void SetCondition::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_condition << m_previousCondition;
}

void SetCondition::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_condition >> m_previousCondition;
}

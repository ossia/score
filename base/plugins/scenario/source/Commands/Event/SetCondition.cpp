#include "SetCondition.hpp"

#include "Document/Event/EventModel.hpp"
#include <State/State.hpp>

#include <QDebug>

using namespace iscore;
using namespace Scenario::Command;


SetCondition::SetCondition(ObjectPath&& eventPath, QString message) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_path {std::move(eventPath) },
m_condition(message)
{
    auto event = m_path.find<EventModel>();
    m_previousCondition = event->condition();
}

void SetCondition::undo()
{
    auto event = m_path.find<EventModel>();
    event->setCondition(m_previousCondition);
}

void SetCondition::redo()
{
    auto event = m_path.find<EventModel>();
    event->setCondition(m_condition);
}

bool SetCondition::mergeWith(const Command* other)
{
    return false;
}

void SetCondition::serializeImpl(QDataStream& s) const
{
    s << m_path << m_condition << m_previousCondition;
}

void SetCondition::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_condition >> m_previousCondition;
}

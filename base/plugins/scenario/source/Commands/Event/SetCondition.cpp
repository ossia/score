#include "SetCondition.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/State/State.hpp"

#include <QDebug>

#define CMD_UID 1001
#define CMD_NAME "SetCondition"
#define CMD_DESC QObject::tr("Set condition of event")

using namespace iscore;
using namespace Scenario::Command;

SetCondition::SetCondition():
	SerializableCommand{"ScenarioControl",
						CMD_NAME,
						CMD_DESC}
{

}

SetCondition::SetCondition(ObjectPath&& eventPath, QString message):
	SerializableCommand{"ScenarioControl",
						CMD_NAME,
						CMD_DESC},
	m_path{std::move(eventPath)},
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

int SetCondition::id() const
{
	return CMD_UID;
}

bool SetCondition::mergeWith(const QUndoCommand* other)
{
	return false;
}

void SetCondition::serializeImpl(QDataStream& s)
{
	s << m_path << m_condition << m_previousCondition;
}

void SetCondition::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_condition >> m_previousCondition;
}

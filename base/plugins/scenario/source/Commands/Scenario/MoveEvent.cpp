#include "MoveEvent.hpp"

#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventData.hpp"

#include <core/application/Application.hpp>
#include <core/view/View.hpp>

#include <QApplication>

using namespace iscore;
using namespace Scenario::Command;

MoveEvent::MoveEvent():
	SerializableCommand{"ScenarioControl",
						"MoveEvent",
						QObject::tr("Event move")}
{
}

MoveEvent::MoveEvent(ObjectPath &&scenarioPath, EventData data):
	SerializableCommand{"ScenarioControl",
						"MoveEvent",
						QObject::tr("Event move")},
	m_path{std::move(scenarioPath)},
	m_eventId{data.eventClickedId},
	m_newHeightPosition{data.relativeY},
	m_newX{data.x}
{
	auto scenar = m_path.find<ScenarioProcessSharedModel>();
	auto ev = scenar->event(m_eventId);
	m_oldHeightPosition = ev->heightPercentage();
	m_oldX = ev->date();
}

void MoveEvent::undo()
{
	auto scenar = m_path.find<ScenarioProcessSharedModel>();

	scenar->moveEventAndConstraint(m_eventId, m_oldX, m_oldHeightPosition);
}

void MoveEvent::redo()
{
	auto scenar = m_path.find<ScenarioProcessSharedModel>();

	scenar->moveEventAndConstraint(m_eventId, m_newX, m_newHeightPosition);
}

int MoveEvent::id() const
{
	return 1;
}

bool MoveEvent::mergeWith(const QUndoCommand* other)
{
	return false;
}

void MoveEvent::serializeImpl(QDataStream& s)
{
	s << m_path << m_eventId
	  << m_oldHeightPosition << m_newHeightPosition << m_oldX << m_newX;
}

void MoveEvent::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_eventId
	  >> m_oldHeightPosition >> m_newHeightPosition >> m_oldX >> m_newX;
}

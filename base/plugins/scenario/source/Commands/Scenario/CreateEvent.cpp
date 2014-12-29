#include "CreateEvent.hpp"

#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventData.hpp"
#include "Document/Constraint/ConstraintModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

CreateEvent::CreateEvent():
	SerializableCommand{"ScenarioControl",
						"CreateEvent",
						QObject::tr("Event creation")},
	m_cmd{new CreateEventAfterEvent}
{

}

CreateEvent::~CreateEvent()
{
	delete m_cmd;
}

CreateEvent::CreateEvent(ObjectPath&& scenarioPath, int time, double heightPosition):
	SerializableCommand{"ScenarioControl",
						"CreateEvent",
						QObject::tr("Event creation")}
{
	auto scenar = scenarioPath.find<ScenarioProcessSharedModel>();

	EventData d;
	d.eventClickedId = (SettableIdentifier::identifier_type) scenar->startEvent()->id();
	d.x = time;
	d.relativeY = heightPosition;

	m_cmd = new CreateEventAfterEvent{std::move(scenarioPath), d};
}

void CreateEvent::undo()
{
	m_cmd->undo();
}

void CreateEvent::redo()
{
	m_cmd->redo();
}

int CreateEvent::id() const
{
	return 1;
}

bool CreateEvent::mergeWith(const QUndoCommand* other)
{
	return false;
}

void CreateEvent::serializeImpl(QDataStream& s)
{
	s << m_cmd->serialize();
}

void CreateEvent::deserializeImpl(QDataStream& s)
{
	QByteArray b;
	s >> b;
	m_cmd->deserialize(b);
}

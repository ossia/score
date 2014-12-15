#include "DeleteEventCommand.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"

#include <core/tools/utilsCPP11.hpp>
using namespace iscore;


DeleteEventCommand::DeleteEventCommand(ObjectPath&& eventPath):
	iscore::SerializableCommand{"ScenarioControl",
								"DeleteEventCommand",
								QObject::tr("Remove event and pre-constraints")},
	m_path{std::move(eventPath)}
{

	auto event = static_cast<EventModel*>(m_path.find());

	{
		QDataStream s(&m_serializedEvent, QIODevice::Append);
		s.setVersion(QDataStream::Qt_5_3);
		s << *event;
	}

	std::vector<ConstraintModel*> v(event->previousConstraints().size());
	int i{-1};
	for(int constraint_id : event->previousConstraints())
	{
		v[++i] = event->parentScenario()->constraint(constraint_id);
	}

	serializeVectorOfPointers(v,
							  m_serializedConstraints);
}

void DeleteEventCommand::undo()
{
}

void DeleteEventCommand::redo()
{
}

int DeleteEventCommand::id() const
{
	return 1;
}

bool DeleteEventCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void DeleteEventCommand::serializeImpl(QDataStream&)
{
}

void DeleteEventCommand::deserializeImpl(QDataStream&)
{
}

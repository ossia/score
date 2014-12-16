#include "DeleteEventCommand.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"

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


	for(const auto& constraint_id : event->previousConstraints())
	{
		auto constraint = event->parentScenario()->constraint(constraint_id);

		QByteArray arr;

		QDataStream s(&arr, QIODevice::Append);
		s.setVersion(QDataStream::Qt_5_3);
		s << *constraint;

		// TODO : see if it's worth it to use a std::vector and emplace instead.
		m_serializedConstraints.push_back(arr);
	}
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

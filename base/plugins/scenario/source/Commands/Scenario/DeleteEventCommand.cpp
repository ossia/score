#include "DeleteEventCommand.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/State/State.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"

#include <core/tools/utilsCPP11.hpp>
using namespace iscore;

EmptyEventCommand::EmptyEventCommand():
	iscore::SerializableCommand{"ScenarioControl",
								"EmptyEventCommand",
								QObject::tr("Remove event and pre-constraints")}
{

}


EmptyEventCommand::EmptyEventCommand(ObjectPath&& eventPath):
	iscore::SerializableCommand{"ScenarioControl",
								"EmptyEventCommand",
								QObject::tr("Remove event and pre-constraints")},
	m_path{std::move(eventPath)}
{

	auto event = static_cast<EventModel*>(m_path.find());

	serializeVectorOfPointers(event->states(),
							  m_serializedStates);




	/* Uncomment for the full deletion mechanism (but it's not what it should do)
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
	*/
}

void EmptyEventCommand::undo()
{
	auto event = static_cast<EventModel*>(m_path.find());
	for(auto& serializedState : m_serializedStates)
	{
		QDataStream s(&serializedState, QIODevice::ReadOnly);
		event->createState(s);
	}

}

void EmptyEventCommand::redo()
{
	auto event = static_cast<EventModel*>(m_path.find());
	for(auto& state : event->states())
	{
		event->removeState(state->id());
	}

}

int EmptyEventCommand::id() const
{
	return 1;
}

bool EmptyEventCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void EmptyEventCommand::serializeImpl(QDataStream& s)
{
	s << m_path << m_serializedStates;
}

void EmptyEventCommand::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_serializedStates;
}

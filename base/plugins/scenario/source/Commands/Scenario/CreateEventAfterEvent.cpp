#include "CreateEventAfterEvent.hpp"

#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Event/EventData.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"
#include "Process/Temporal/TemporalScenarioProcessViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;


CreateEventAfterEvent::CreateEventAfterEvent():
	CreateEventAfterEvent{{}, {}}
{
}

CreateEventAfterEvent::CreateEventAfterEvent(ObjectPath &&scenarioPath, EventData data):
	SerializableCommand{"ScenarioControl",
						"CreateEventAfterEvent",
						QObject::tr("Event creation")},
	m_path{std::move(scenarioPath)},
	m_firstEventId{data.eventClickedId},
	m_time{data.x},
	m_heightPosition{data.relativeY}
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());

	m_createdEventId = getNextId(scenar->events());
	m_createdConstraintId = getNextId(scenar->constraints());
	m_createdBoxId = getNextId();


	// For each ScenarioViewModel of the scenario we are applying this command in,
	// we have to generate ConstraintViewModels, too
	for(auto& viewModel : viewModels(scenar))
	{
		m_createdConstraintViewModelIDs[identifierOfViewModelFromSharedModel(viewModel)] = getNextId(viewModel->constraints());
	}
}

void CreateEventAfterEvent::undo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());

	scenar->undo_createConstraintAndEndEventFromEvent(m_createdConstraintId);
}

void CreateEventAfterEvent::redo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());

	scenar->createConstraintAndEndEventFromEvent(m_firstEventId,
												 m_time,
												 m_heightPosition,
												 m_createdConstraintId,
												 m_createdEventId);

	// Creation of all the constraint view models
	for(auto& viewModel : viewModels(scenar))
	{
		auto cvm_id = identifierOfViewModelFromSharedModel(viewModel);
		if(m_createdConstraintViewModelIDs.contains(cvm_id))
		{
			viewModel->makeConstraintViewModel(m_createdConstraintId,
											   m_createdConstraintViewModelIDs[cvm_id]);
		}
		else
		{
			throw std::runtime_error("CreateEvent : missing identifier.");
		}
	}

	// TODO Creation of all the event view models
}

int CreateEventAfterEvent::id() const
{
	return 1;
}

bool CreateEventAfterEvent::mergeWith(const QUndoCommand* other)
{
	return false;
}

void CreateEventAfterEvent::serializeImpl(QDataStream& s)
{
	s << m_path << m_firstEventId << m_time;
}

void CreateEventAfterEvent::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_firstEventId >> m_time;
}

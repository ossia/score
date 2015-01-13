#include "CreateEventAfterEvent.hpp"

#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Event/EventData.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"
#include "Process/Temporal/TemporalScenarioProcessViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;


CreateEventAfterEvent::CreateEventAfterEvent():
	SerializableCommand{"ScenarioControl",
						"CreateEventAfterEvent",
						QObject::tr("Event creation")}
{
}

CreateEventAfterEvent::CreateEventAfterEvent(ObjectPath &&scenarioPath, EventData data):
	SerializableCommand{"ScenarioControl",
						"CreateEventAfterEvent",
						QObject::tr("Event creation")},
	m_path{std::move(scenarioPath)},
	m_firstEventId{data.eventClickedId},
	m_time{data.dDate},
	m_heightPosition{data.relativeY}
{
	auto scenar = m_path.find<ScenarioProcessSharedModel>();

	m_createdEventId = getNextId(scenar->events());
	m_createdConstraintId = getNextId(scenar->constraints());
	m_createdTimeNodeId = getNextId(scenar->timeNodes());

	// For each ScenarioViewModel of the scenario we are applying this command in,
	// we have to generate ConstraintViewModels, too
	for(auto& viewModel : viewModels(scenar))
	{
		m_createdConstraintViewModelIDs[identifierOfViewModelFromSharedModel(viewModel)] = getNextId(viewModel->constraints());
	}
}

void CreateEventAfterEvent::undo()
{
	auto scenar = m_path.find<ScenarioProcessSharedModel>();

	scenar->undo_createConstraintAndEndEventFromEvent(m_createdConstraintId);
}

void CreateEventAfterEvent::redo()
{
	auto scenar = m_path.find<ScenarioProcessSharedModel>();

	scenar->createConstraintAndEndEventFromEvent(m_firstEventId,
												 m_time,
												 m_heightPosition,
												 m_createdConstraintId,
												 m_createdEventId,
												 m_createdTimeNodeId);

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
	// @todo Creation of all the event view models
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
	s << m_path
	  << m_firstEventId
	  << m_time
	  << m_heightPosition
	  << m_createdEventId
	  << m_createdConstraintId
	  << m_createdConstraintViewModelIDs;
}

void CreateEventAfterEvent::deserializeImpl(QDataStream& s)
{
	s >> m_path
	  >> m_firstEventId
	  >> m_time
	  >> m_heightPosition
	  >> m_createdEventId
	  >> m_createdConstraintId
	  >> m_createdConstraintViewModelIDs;
}

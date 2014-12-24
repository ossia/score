#include "CreateEvent.hpp"

#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"
#include "Process/Temporal/TemporalScenarioProcessViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

CreateEvent::CreateEvent():
	SerializableCommand{"ScenarioControl",
						"CreateEventCommand",
						QObject::tr("Event creation")}
{

}

CreateEvent::CreateEvent(ObjectPath&& scenarioPath, int time, double heightPosition):
	SerializableCommand{"ScenarioControl",
						"CreateEventCommand",
						QObject::tr("Event creation")},
	m_path(std::move(scenarioPath)),
	m_time{time},
	m_heightPosition{heightPosition}
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());

	m_createdConstraintId = getNextId(scenar->constraints());
	m_createdEventId = getNextId(scenar->events());

	// For each ScenarioViewModel of the scenario we are applying this command in,
	// we have to generate ConstraintViewModels, too
	for(auto& viewModel : viewModels(scenar))
	{
		m_createdConstraintViewModelIDs[identifierOfViewModelFromSharedModel(viewModel)] = getNextId(viewModel->constraints());
	}
}

void CreateEvent::undo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());

	scenar->undo_createConstraintAndEndEventFromStartEvent(m_createdConstraintId);

	// The view models will be removed, since they have no meaning without a ConstraintModel
}

void CreateEvent::redo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());

	// Creation of the constraint and event model
	scenar->createConstraintAndEndEventFromEvent(scenar->startEvent()->id(),
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
	s << m_path
	  << m_createdConstraintId
	  << m_createdEventId
	  << m_createdConstraintViewModelIDs
	  << m_time
	  << m_heightPosition;
}

void CreateEvent::deserializeImpl(QDataStream& s)
{
	s >> m_path
	  >> m_createdConstraintId
	  >> m_createdEventId
	  >> m_createdConstraintViewModelIDs
	  >> m_time
	  >> m_heightPosition;
}

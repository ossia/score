#include "CreateEventCommand.hpp"

#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"
#include "Process/Temporal/TemporalScenarioProcessViewModel.hpp"

#include <core/application/Application.hpp>
#include <core/view/View.hpp>

#include <QApplication>
#include <QDebug>
using namespace iscore;

CreateEventCommand::CreateEventCommand():
	SerializableCommand{"ScenarioControl",
						"CreateEventCommand",
						QObject::tr("Event creation")}
{

}

CreateEventCommand::CreateEventCommand(ObjectPath&& scenarioPath, int time, double heightPosition):
	SerializableCommand{"ScenarioControl",
						"CreateEventCommand",
						QObject::tr("Event creation")},
	m_path(std::move(scenarioPath)),
	m_time{time},
	m_heightPosition{heightPosition}
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());

	m_createdConstraintId = getNextId(scenar->constraints());
	m_createdBoxId = getNextId();
	m_createdEventId = getNextId(scenar->events());

	// For each ScenarioViewModel of the scenario we are applying this command in,
	// we have to generate ConstraintViewModels, too
	for(auto& viewModel : viewModels(scenar))
	{
		m_createdConstraintViewModelIDs[identifierOfViewModelFromSharedModel(viewModel)] = getNextId(viewModel->constraints());
	}
}

void CreateEventCommand::undo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());

	scenar->undo_createConstraintAndEndEventFromStartEvent(m_createdConstraintId);

	// The view models will be removed, since they have no meaning without a ConstraintModel
}

void CreateEventCommand::redo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_path.find());

	// Creation of the constraint and event model
	scenar->createConstraintAndEndEventFromStartEvent(m_time,
													  m_heightPosition,
													  m_createdConstraintId,
													  m_createdEventId);

	// Creation of all the constraint view models
	for(auto& viewModel : viewModels(scenar))
	{
		auto cvm_id = identifierOfViewModelFromSharedModel(viewModel);
		if(m_createdConstraintViewModelIDs.contains(cvm_id))
		{
			viewModel->createConstraintViewModel(m_createdConstraintId,
												 m_createdConstraintViewModelIDs[cvm_id]);
		}
	}

	// TODO Creation of all the event view models
}

int CreateEventCommand::id() const
{
	return 1;
}

bool CreateEventCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void CreateEventCommand::serializeImpl(QDataStream& s)
{
	s << m_path
	  << m_createdConstraintId
	  << m_createdBoxId
	  << m_createdEventId
	  << m_createdConstraintViewModelIDs
	  << m_time
	  << m_heightPosition;
}

void CreateEventCommand::deserializeImpl(QDataStream& s)
{
	s >> m_path
	  >> m_createdConstraintId
	  >> m_createdBoxId
	  >> m_createdEventId
	  >> m_createdConstraintViewModelIDs
	  >> m_time
	  >> m_heightPosition;
}

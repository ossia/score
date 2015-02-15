#include "CreateEventAfterEvent.hpp"

#include "source/Process/ScenarioModel.hpp"
#include "source/Document/Event/EventModel.hpp"
#include "source/Document/Constraint/ConstraintModel.hpp"
#include "source/Document/Event/EventData.hpp"
#include "source/Document/TimeNode/TimeNodeModel.hpp"
#include "source/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "source/Process/Temporal/TemporalScenarioViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

#define CMD_UID 1204

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
	auto scenar = m_path.find<ScenarioModel>();

	m_createdEventId = getStrongId(scenar->events());
	m_createdConstraintId = getStrongId(scenar->constraints());

    if (*data.endTimeNodeId.val() == 0)
    {
		m_timeNodeId = getStrongId(scenar->timeNodes());
		m_timeNodeToCreate = true;
    }
    else
    {
		m_timeNodeId = data.endTimeNodeId ;
		m_timeNodeToCreate = false;
    }

    // For each ScenarioViewModel of the scenario we are applying this command in,
	// we have to generate ConstraintViewModels, too
	for(auto& viewModel : viewModels(scenar))
	{
		m_createdConstraintViewModelIDs[identifierOfViewModelFromSharedModel(viewModel)] = getStrongId(viewModel->constraints());
	}

	// Finally, the id of the full view
	m_createdConstraintFullViewId = getStrongId(m_createdConstraintViewModelIDs.values().toVector().toStdVector());
}

void CreateEventAfterEvent::undo()
{
	auto scenar = m_path.find<ScenarioModel>();

    scenar->undo_createConstraintAndEndEventFromEvent(m_createdEventId);
}

void CreateEventAfterEvent::redo()
{
	auto scenar = m_path.find<ScenarioModel>();

	if (! m_timeNodeToCreate)
    {
		scenar->timeNode(m_timeNodeId)->addEvent(m_createdEventId);
    }

	scenar->createConstraintAndEndEventFromEvent(m_firstEventId,
												 m_time,
												 m_heightPosition,
												 m_createdConstraintId,
												 m_createdConstraintFullViewId,
                                                 m_createdEventId);
	if (m_timeNodeToCreate)
    {
		scenar->createTimeNode(m_timeNodeId, m_createdEventId);
		scenar->event(m_createdEventId)->changeTimeNode(m_timeNodeId);
    }
    else
    {
		scenar->event(m_createdEventId)->changeTimeNode(m_timeNodeId);
    }

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
	return canMerge() ? CMD_UID : -1;
}

bool CreateEventAfterEvent::mergeWith(const QUndoCommand* other)
{
	// Maybe set m_mergeable = false at the end ?
	if(other->id() != id())
		return false;

	auto cmd = static_cast<const CreateEventAfterEvent*>(other);
	m_time = cmd->m_time;
	m_heightPosition = cmd->m_heightPosition;
	m_timeNodeId = cmd->m_timeNodeId;
	m_timeNodeToCreate = cmd->m_timeNodeToCreate;

	return true;
}

void CreateEventAfterEvent::serializeImpl(QDataStream& s) const
{
	s << m_path
	  << m_firstEventId
	  << m_time
	  << m_heightPosition
	  << m_createdEventId
	  << m_createdConstraintId
	  << m_timeNodeId
	  << m_timeNodeToCreate
	  << m_createdConstraintViewModelIDs
	  << m_createdConstraintFullViewId;
}

void CreateEventAfterEvent::deserializeImpl(QDataStream& s)
{
	s >> m_path
	  >> m_firstEventId
	  >> m_time
	  >> m_heightPosition
	  >> m_createdEventId
	  >> m_createdConstraintId
	  >> m_timeNodeId
	  >> m_timeNodeToCreate
	  >> m_createdConstraintViewModelIDs
	  >> m_createdConstraintFullViewId;
}

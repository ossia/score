#include "CreateEventAfterEvent.hpp"

#include "source/Process/ScenarioProcessSharedModel.hpp"
#include "source/Document/Event/EventModel.hpp"
#include "source/Document/Constraint/ConstraintModel.hpp"
#include "source/Document/Event/EventData.hpp"
#include "source/Document/TimeNode/TimeNodeModel.hpp"
#include "source/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "source/Process/Temporal/TemporalScenarioProcessViewModel.hpp"

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

	m_createdEventId = getStrongId(scenar->events());
	m_createdConstraintId = getStrongId(scenar->constraints());

    if (*data.endTimeNodeId.val() == 0)
    {
        m_TimeNodeId = getStrongId(scenar->timeNodes());
        timeNodeToCreate = true;
    }
    else
    {
        m_TimeNodeId = data.endTimeNodeId ;
        timeNodeToCreate = false;
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
	auto scenar = m_path.find<ScenarioProcessSharedModel>();

    scenar->undo_createConstraintAndEndEventFromEvent(m_createdEventId);
}

void CreateEventAfterEvent::redo()
{
	auto scenar = m_path.find<ScenarioProcessSharedModel>();

    if (! timeNodeToCreate)
    {
        scenar->timeNode(m_TimeNodeId)->addEvent(m_createdEventId);
    }

	scenar->createConstraintAndEndEventFromEvent(m_firstEventId,
												 m_time,
												 m_heightPosition,
												 m_createdConstraintId,
												 m_createdConstraintFullViewId,
                                                 m_createdEventId);
    if (timeNodeToCreate)
    {
        scenar->createTimeNode(m_TimeNodeId, m_createdEventId);
        scenar->event(m_createdEventId)->changeTimeNode(m_TimeNodeId);
    }
    else
    {
        scenar->event(m_createdEventId)->changeTimeNode(m_TimeNodeId);
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
	return 1;
}

bool CreateEventAfterEvent::mergeWith(const QUndoCommand* other)
{
	return false;
}

void CreateEventAfterEvent::serializeImpl(QDataStream& s) const
{
	s << m_path
	  << m_firstEventId
	  << m_time
	  << m_heightPosition
	  << m_createdEventId
	  << m_createdConstraintId
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
	  >> m_createdConstraintViewModelIDs
	  >> m_createdConstraintFullViewId;
}

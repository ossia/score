#include "CreateEventAfterEventOnTimeNode.hpp"

#include "source/Process/ScenarioModel.hpp"
#include "source/Document/Event/EventModel.hpp"
#include "source/Document/Constraint/ConstraintModel.hpp"
#include "source/Document/Event/EventData.hpp"
#include "source/Document/TimeNode/TimeNodeModel.hpp"
#include "source/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "source/Process/Temporal/TemporalScenarioViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

CreateEventAfterEventOnTimeNode::CreateEventAfterEventOnTimeNode(ObjectPath&& scenarioPath, EventData data) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_path {std::move(scenarioPath) },
m_timeNodeId {data.endTimeNodeId},
m_firstEventId {data.eventClickedId},
m_time {data.dDate},
m_heightPosition {data.relativeY}
{
    auto scenar = m_path.find<ScenarioModel>();

    m_createdEventId = getStrongId(scenar->events());
    m_createdConstraintId = getStrongId(scenar->constraints());

    // For each ScenarioViewModel of the scenario we are applying this command in,
    // we have to generate ConstraintViewModels, too
    for(auto& viewModel : viewModels(scenar))
    {
        m_createdConstraintViewModelIDs[identifierOfViewModelFromSharedModel(viewModel)] = getStrongId(viewModel->constraints());
    }

    // Finally, the id of the full view
    m_createdConstraintFullViewId = getStrongId(m_createdConstraintViewModelIDs.values().toVector().toStdVector());
}

void CreateEventAfterEventOnTimeNode::undo()
{
    auto scenar = m_path.find<ScenarioModel>();

    // TODO enlever event de la timenode?
    scenar->undo_createConstraintAndEndEventFromEvent(m_createdEventId);
}

void CreateEventAfterEventOnTimeNode::redo()
{
    auto scenar = m_path.find<ScenarioModel>();


    scenar->createConstraintAndEndEventFromEvent(m_firstEventId,
            m_time,
            m_heightPosition,
            m_createdConstraintId,
            m_createdConstraintFullViewId,
            m_createdEventId);

    scenar->timeNode(m_timeNodeId)->addEvent(m_createdEventId);

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

int CreateEventAfterEventOnTimeNode::id() const
{
    return canMerge() ? uid() : -1;
}

bool CreateEventAfterEventOnTimeNode::mergeWith(const QUndoCommand* other)
{
    // Maybe set m_mergeable = false at the end ?
    if(other->id() != id())
    {
        return false;
    }

    auto cmd = static_cast<const CreateEventAfterEventOnTimeNode*>(other);
    m_time = cmd->m_time;
    m_heightPosition = cmd->m_heightPosition;

    return true;
}

void CreateEventAfterEventOnTimeNode::serializeImpl(QDataStream& s) const
{
    s << m_path
      << m_firstEventId
      << m_time
      << m_heightPosition
      << m_createdEventId
      << m_createdConstraintId
      << m_timeNodeId
      << m_createdConstraintViewModelIDs
      << m_createdConstraintFullViewId;
}

void CreateEventAfterEventOnTimeNode::deserializeImpl(QDataStream& s)
{
    s >> m_path
      >> m_firstEventId
      >> m_time
      >> m_heightPosition
      >> m_createdEventId
      >> m_createdConstraintId
      >> m_timeNodeId
      >> m_createdConstraintViewModelIDs
      >> m_createdConstraintFullViewId;
}

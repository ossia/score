#include "CreateEventAfterEvent.hpp"

#include "source/Process/ScenarioModel.hpp"
#include "source/Document/Event/EventModel.hpp"
#include "source/Document/Constraint/ConstraintModel.hpp"
#include "source/Document/Event/EventData.hpp"
#include "source/Document/TimeNode/TimeNodeModel.hpp"
#include "source/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "source/Process/Temporal/TemporalScenarioViewModel.hpp"
#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"
#include "Process/Algorithms/StandardDisplacementPolicy.hpp"

#include "source/Commands/Scenario/Displacement/MoveEvent.hpp"

using namespace iscore;
using namespace Scenario::Command;

CreateEventAfterEvent::CreateEventAfterEvent(ObjectPath&& scenarioPath,
                                             id_type<EventModel> firstEvent,
                                             const TimeValue& date,
                                             double y) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_path {std::move(scenarioPath) },
    m_firstEventId {firstEvent},
    m_time {date},
    m_heightPosition {y}
{
    auto scenar = m_path.find<ScenarioModel>();

    m_createdEventId = getStrongId(scenar->events());
    m_createdConstraintId = getStrongId(scenar->constraints());
    m_createdTimeNodeId = getStrongId(scenar->timeNodes());

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
    StandardRemovalPolicy::removeEvent(*scenar, m_createdEventId);
}

void CreateEventAfterEvent::redo()
{
    auto scenar = m_path.find<ScenarioModel>();
    createTimenodeConstraintAndEvent(m_createdConstraintId,
                                     m_createdConstraintFullViewId,
                                     *scenar->startEvent(),
                                     m_createdEventId,
                                     m_createdTimeNodeId,
                                     m_time,
                                     m_heightPosition,
                                     *scenar);

    createConstraintViewModels(m_createdConstraintViewModelIDs,
                               m_createdConstraintId,
                               scenar);
}

bool CreateEventAfterEvent::mergeWith(const Command* other)
{
    // Maybe set m_mergeable = false at the end ?
    if(other->uid() != uid())
    {
        return false;
    }

    auto cmd = static_cast<const CreateEventAfterEvent*>(other);
    m_time = cmd->m_time;
    m_heightPosition = cmd->m_heightPosition;

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
      << m_createdTimeNodeId
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
            >> m_createdTimeNodeId
            >> m_createdConstraintViewModelIDs
            >> m_createdConstraintFullViewId;
}

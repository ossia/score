#include "CreateConstraint.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Event/EventData.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"

using namespace iscore;
using namespace Scenario::Command;

CreateConstraint::CreateConstraint(ObjectPath&& scenarioPath,
                                   id_type<EventModel> startEvent,
                                   id_type<EventModel> endEvent) :
    SerializableCommand{"ScenarioControl",
                        className(),
                        description()},
    m_path {std::move(scenarioPath) },
    m_startEventId {startEvent},
    m_endEventId {endEvent}
{
    auto scenar = m_path.find<ScenarioModel>();
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

void CreateConstraint::undo()
{
    auto scenar = m_path.find<ScenarioModel>();

    StandardRemovalPolicy::removeConstraint(*scenar, m_createdConstraintId);
}

void CreateConstraint::redo()
{
    auto scenar = m_path.find<ScenarioModel>();

    StandardCreationPolicy::createConstraintBetweenEvents(
                *scenar,
                m_startEventId,
                m_endEventId,
                m_createdConstraintId,
                m_createdConstraintFullViewId);

    createConstraintViewModels(m_createdConstraintViewModelIDs,
                               m_createdConstraintId,
                               scenar);
}

bool CreateConstraint::mergeWith(const Command* other)
{
    // Maybe set m_mergeable = false at the end ?
    if(other->uid() != uid())
    {
        return false;
    }

    auto cmd = static_cast<const CreateConstraint*>(other);
    m_endEventId = cmd->m_endEventId;

    return true;
}

void CreateConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path
      << m_startEventId
      << m_endEventId
      << m_createdConstraintId
      << m_createdConstraintViewModelIDs
      << m_createdConstraintFullViewId;
}

void CreateConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path
            >> m_startEventId
            >> m_endEventId
            >> m_createdConstraintId
            >> m_createdConstraintViewModelIDs
            >> m_createdConstraintFullViewId;
}

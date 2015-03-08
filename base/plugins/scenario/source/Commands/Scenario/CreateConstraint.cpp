#include "CreateConstraint.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Event/EventData.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "Process/Algorithms/StandardCreationPolicy.hpp"

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
}

void CreateConstraint::undo()
{
    auto scenar = m_path.find<ScenarioModel>();

    scenar->removeConstraint(m_createdConstraintId);
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

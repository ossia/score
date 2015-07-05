#include "CreateConstraint.hpp"

#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
using namespace iscore;
using namespace Scenario::Command;

CreateConstraint::CreateConstraint(
        ObjectPath&& scenarioPath,
        const id_type<StateModel>& startState,
        const id_type<StateModel>& endState) :
    SerializableCommand{"ScenarioControl",
                        commandName(),
                        description()},
    m_path {std::move(scenarioPath) },
    m_startStateId{startState},
    m_endStateId{endState}
{
    auto& scenar = m_path.find<ScenarioModel>();
    m_createdConstraintId = getStrongId(scenar.constraints());

    // For each ScenarioViewModel of the scenario we are applying this command in,
    // we have to generate ConstraintViewModels, too
    for(auto& viewModel : layers(scenar))
    {
        m_createdConstraintViewModelIDs[iscore::IDocument::path(viewModel)] = getStrongId(viewModel->constraints());
    }

    // Finally, the id of the full view
    m_createdConstraintFullViewId = getStrongId(m_createdConstraintViewModelIDs.values().toVector().toStdVector());
}

void CreateConstraint::undo()
{
    auto& scenar = m_path.find<ScenarioModel>();

    ScenarioCreate<ConstraintModel>::undo(m_createdConstraintId, scenar);
}

void CreateConstraint::redo()
{
    auto& scenar = m_path.find<ScenarioModel>();
    auto& sst = scenar.state(m_startStateId);
    auto& est = scenar.state(m_endStateId);

    ScenarioCreate<ConstraintModel>::redo(
                m_createdConstraintId,
                m_createdConstraintFullViewId,
                sst,
                est,
                sst.heightPercentage(),
                scenar);

    createConstraintViewModels(m_createdConstraintViewModelIDs,
                               m_createdConstraintId,
                               scenar);
}



void CreateConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path
      << m_createdConstraintId
      << m_startStateId
      << m_endStateId
      << m_createdConstraintViewModelIDs
      << m_createdConstraintFullViewId;
}

void CreateConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path
            >> m_createdConstraintId
            >> m_startStateId
            >> m_endStateId
            >> m_createdConstraintViewModelIDs
            >> m_createdConstraintFullViewId;
}

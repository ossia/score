#include "CreateConstraint.hpp"

#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Process/Temporal/TemporalScenarioLayerModel.hpp"
#include "Process/Algorithms/StandardCreationPolicy.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include "Tools/RandomNameProvider.hpp"
using namespace iscore;
using namespace Scenario::Command;

CreateConstraint::CreateConstraint(
        Path<ScenarioModel>&& scenarioPath,
        const Id<StateModel>& startState,
        const Id<StateModel>& endState) :
    SerializableCommand{"ScenarioControl",
                        commandName(),
                        description()},
    m_path {std::move(scenarioPath) },
    m_createdName{RandomNameProvider::generateRandomName()},
    m_startStateId{startState},
    m_endStateId{endState}
{
    auto& scenar = m_path.find();
    m_createdConstraintId = getStrongId(scenar.constraints);

    // For each ScenarioViewModel of the scenario we are applying this command in,
    // we have to generate ConstraintViewModels, too
    for(const auto& viewModel : layers(scenar))
    {
        m_createdConstraintViewModelIDs[*viewModel] = getStrongId(viewModel->constraints());
    }

    // Finally, the id of the full view
    m_createdConstraintFullViewId = getStrongId(m_createdConstraintViewModelIDs.values().toVector().toStdVector());
}

void CreateConstraint::undo()
{
    auto& scenar = m_path.find();

    ScenarioCreate<ConstraintModel>::undo(m_createdConstraintId, scenar);
}

void CreateConstraint::redo()
{
    auto& scenar = m_path.find();
    auto& sst = scenar.states.at(m_startStateId);
    auto& est = scenar.states.at(m_endStateId);

    ScenarioCreate<ConstraintModel>::redo(
                m_createdConstraintId,
                m_createdConstraintFullViewId,
                sst,
                est,
                sst.heightPercentage(),
                scenar);

    scenar.constraints.at(m_createdConstraintId).metadata.setName(m_createdName);

    createConstraintViewModels(m_createdConstraintViewModelIDs,
                               m_createdConstraintId,
                               scenar);
}



void CreateConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path
      << m_createdName
      << m_createdConstraintId
      << m_startStateId
      << m_endStateId
      << m_createdConstraintViewModelIDs
      << m_createdConstraintFullViewId;
}

void CreateConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path
            >> m_createdName
            >> m_createdConstraintId
            >> m_startStateId
            >> m_endStateId
            >> m_createdConstraintViewModelIDs
            >> m_createdConstraintFullViewId;
}

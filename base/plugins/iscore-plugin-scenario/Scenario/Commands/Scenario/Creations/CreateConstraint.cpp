#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/tools/RandomNameProvider.hpp>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <QList>
#include <QVector>
#include <algorithm>
#include <vector>

#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include "CreateConstraint.hpp"
#include <Process/ModelMetadata.hpp>
#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/AbstractScenarioLayerModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

using namespace iscore;
using namespace Scenario::Command;

CreateConstraint::CreateConstraint(
        Path<Scenario::ScenarioModel>&& scenarioPath,
        const Id<StateModel>& startState,
        const Id<StateModel>& endState) :
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

void CreateConstraint::undo() const
{
    auto& scenar = m_path.find();

    ScenarioCreate<ConstraintModel>::undo(m_createdConstraintId, scenar);
}

void CreateConstraint::redo() const
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



void CreateConstraint::serializeImpl(DataStreamInput& s) const
{
    s << m_path
      << m_createdName
      << m_createdConstraintId
      << m_startStateId
      << m_endStateId
      << m_createdConstraintViewModelIDs
      << m_createdConstraintFullViewId;
}

void CreateConstraint::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path
            >> m_createdName
            >> m_createdConstraintId
            >> m_startStateId
            >> m_endStateId
            >> m_createdConstraintViewModelIDs
            >> m_createdConstraintFullViewId;
}

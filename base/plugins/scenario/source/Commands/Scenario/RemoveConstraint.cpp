#include "RemoveConstraint.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "source/ProcessInterfaceSerialization/ProcessSharedModelInterfaceSerialization.hpp"
#include "Process/Temporal/TemporalScenarioProcessViewModel.hpp"

#include <core/tools/utilsCPP11.hpp>

using namespace iscore;
using namespace Scenario::Command;

RemoveConstraint::RemoveConstraint():
	SerializableCommand{"ScenarioControl",
                        "RemoveConstraint",
						QObject::tr("Remove event and pre-constraints")}
{
}


RemoveConstraint::RemoveConstraint(ObjectPath&& scenarioPath, ConstraintModel* constraint):
	SerializableCommand{"ScenarioControl",
                        "RemoveConstraint",
						QObject::tr("Remove event and pre-constraints")},
    m_path{std::move(scenarioPath)}
{
    QByteArray arr;
    Serializer<DataStream> s{&arr};
    s.readFrom(*constraint);
    m_serializedConstraint = arr;

    m_cstrId = constraint->id();

    auto scenar = m_path.find<ScenarioProcessSharedModel>();
    for(auto& viewModel : viewModels(scenar))
    {
        // todo : associer toutes les combinaisons de modèles de vues, pas que la première =)
        auto cstrVM = constraint->viewModels().first();
        auto cvm_id = identifierOfViewModelFromSharedModel(viewModel);
        m_constraintViewModelIDs[cvm_id] = cstrVM->id();
    }
}

void RemoveConstraint::undo()
{
    auto scenar = m_path.find<ScenarioProcessSharedModel>();

    Deserializer<DataStream> s{&m_serializedConstraint};

    scenar->undo_removeConstraint(new ConstraintModel(s, scenar));

    for(auto& viewModel : viewModels(scenar))
    {
       //*
        auto cvm_id = identifierOfViewModelFromSharedModel(viewModel);
        if(m_constraintViewModelIDs.contains(cvm_id))
        {
            viewModel->makeConstraintViewModel(m_cstrId,
                                               m_constraintViewModelIDs[cvm_id]);
        }
        else
        {
            throw std::runtime_error("undo RemoveConstraint : missing identifier.");
        }
        //*/
    }
}

void RemoveConstraint::redo()
{
    auto scenar = m_path.find<ScenarioProcessSharedModel>();
    scenar->removeConstraint(m_cstrId);
}

int RemoveConstraint::id() const
{
	return 1;
}

bool RemoveConstraint::mergeWith(const QUndoCommand* other)
{
	return false;
}

void RemoveConstraint::serializeImpl(QDataStream& s)
{
    s << m_path << m_cstrId << m_serializedConstraint << m_constraintViewModelIDs << m_constraintFullViewId << m_startEvent << m_endEvent;
}

void RemoveConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_cstrId >> m_serializedConstraint >> m_constraintViewModelIDs >> m_constraintFullViewId >> m_startEvent >> m_endEvent ;
}

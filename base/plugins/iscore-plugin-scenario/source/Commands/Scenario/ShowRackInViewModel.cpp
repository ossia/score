#include "ShowRackInViewModel.hpp"
#include "Document/Constraint/ViewModels/ConstraintViewModel.hpp"
#include "iscore/document/DocumentInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

ShowRackInViewModel::ShowRackInViewModel(
        ModelPath<ConstraintViewModel>&& constraint_path,
        id_type<RackModel> rackId) :
    SerializableCommand{"ScenarioControl",
                        commandName(),
                        description()},
    m_constraintViewModelPath {std::move(constraint_path) },
    m_rackId {rackId}
{
    auto& vm = m_constraintViewModelPath.find();
    m_previousRackId = vm.shownRack();
}

ShowRackInViewModel::ShowRackInViewModel(
        const ConstraintViewModel& vm,
        const id_type<RackModel>& rackId) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_constraintViewModelPath {iscore::IDocument::safe_path(vm)},
    m_rackId {rackId}
{
    m_previousRackId = vm.shownRack();
}

void ShowRackInViewModel::undo()
{
    auto& vm = m_constraintViewModelPath.find();

    if(m_previousRackId.val())
    {
        vm.showRack(m_previousRackId);
    }
    else
    {
        vm.hideRack();
    }
}

void ShowRackInViewModel::redo()
{
    auto& vm = m_constraintViewModelPath.find();
    vm.showRack(m_rackId);
}

void ShowRackInViewModel::serializeImpl(QDataStream& s) const
{
    s << m_constraintViewModelPath
      << m_rackId
      << m_previousRackId;
}

void ShowRackInViewModel::deserializeImpl(QDataStream& s)
{
    s >> m_constraintViewModelPath
            >> m_rackId
            >> m_previousRackId;
}

#include "HideRackInViewModel.hpp"
#include "Document/Constraint/ViewModels/ConstraintViewModel.hpp"
#include "iscore/document/DocumentInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

HideRackInViewModel::HideRackInViewModel(
        ModelPath<ConstraintViewModel>&& path) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_constraintViewModelPath {std::move(path) }
{
    auto& constraint_vm = m_constraintViewModelPath.find();
    m_constraintPreviousId = constraint_vm.shownRack();
}

HideRackInViewModel::HideRackInViewModel(
        const ConstraintViewModel& constraint_vm) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_constraintViewModelPath {iscore::IDocument::safe_path(constraint_vm) }
{
    m_constraintPreviousId = constraint_vm.shownRack();
}

void HideRackInViewModel::undo()
{
    auto& vm = m_constraintViewModelPath.find();
    vm.showRack(m_constraintPreviousId);
}

void HideRackInViewModel::redo()
{
    auto& vm = m_constraintViewModelPath.find();
    vm.hideRack();
}

void HideRackInViewModel::serializeImpl(QDataStream& s) const
{
    s << m_constraintViewModelPath
      << m_constraintPreviousId;
}

void HideRackInViewModel::deserializeImpl(QDataStream& s)
{
    s >> m_constraintViewModelPath
            >> m_constraintPreviousId;
}

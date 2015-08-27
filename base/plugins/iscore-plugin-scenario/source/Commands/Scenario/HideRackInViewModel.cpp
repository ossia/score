#include "HideRackInViewModel.hpp"
#include "Document/Constraint/ViewModels/ConstraintViewModel.hpp"
#include "iscore/document/DocumentInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

HideRackInViewModel::HideRackInViewModel(ObjectPath&& path) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
m_constraintViewModelPath {std::move(path) }
{
    auto& constraint_vm = m_constraintViewModelPath.find<ConstraintViewModel>();
    m_constraintPreviousId = constraint_vm.shownRack();
}

HideRackInViewModel::HideRackInViewModel(ConstraintViewModel* constraint_vm) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
m_constraintViewModelPath {iscore::IDocument::unsafe_path(constraint_vm) }
{
    m_constraintPreviousId = constraint_vm->shownRack();
}

void HideRackInViewModel::undo()
{
    auto& vm = m_constraintViewModelPath.find<ConstraintViewModel>();
    vm.showRack(m_constraintPreviousId);
}

void HideRackInViewModel::redo()
{
    auto& vm = m_constraintViewModelPath.find<ConstraintViewModel>();
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

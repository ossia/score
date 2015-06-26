#include "HideRackInViewModel.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"
#include "iscore/document/DocumentInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

HideRackInViewModel::HideRackInViewModel(ObjectPath&& path) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
m_constraintViewModelPath {std::move(path) }
{
    auto& constraint_vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();
    m_constraintPreviousId = constraint_vm.shownRack();
}

HideRackInViewModel::HideRackInViewModel(AbstractConstraintViewModel* constraint_vm) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
m_constraintViewModelPath {iscore::IDocument::path(constraint_vm) }
{
    m_constraintPreviousId = constraint_vm->shownRack();
}

void HideRackInViewModel::undo()
{
    auto& vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();
    vm.showRack(m_constraintPreviousId);
}

void HideRackInViewModel::redo()
{
    auto& vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();
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

#include "HideBoxInViewModel.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"
#include "iscore/document/DocumentInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

HideBoxInViewModel::HideBoxInViewModel(ObjectPath&& path) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_constraintViewModelPath {std::move(path) }
{
    auto constraint_vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();
    m_constraintPreviousId = constraint_vm->shownBox();
}

HideBoxInViewModel::HideBoxInViewModel(AbstractConstraintViewModel* constraint_vm) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_constraintViewModelPath {iscore::IDocument::path(constraint_vm) }
{
    m_constraintPreviousId = constraint_vm->shownBox();
}

void HideBoxInViewModel::undo()
{
    auto vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();
    vm->showBox(m_constraintPreviousId);
}

void HideBoxInViewModel::redo()
{
    auto vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();
    vm->hideBox();
}

void HideBoxInViewModel::serializeImpl(QDataStream& s) const
{
    s << m_constraintViewModelPath
      << m_constraintPreviousId;
}

void HideBoxInViewModel::deserializeImpl(QDataStream& s)
{
    s >> m_constraintViewModelPath
      >> m_constraintPreviousId;
}

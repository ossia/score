#include "ShowBoxInViewModel.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"
#include "core/interface/document/DocumentInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

ShowBoxInViewModel::ShowBoxInViewModel(ObjectPath&& constraint_path,
                                       id_type<BoxModel> boxId) :
    SerializableCommand{"ScenarioControl",
                        className(),
                        description()},
m_constraintViewModelPath {std::move(constraint_path) },
m_boxId {boxId}
{
    auto constraint_vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();
    m_previousBoxId = constraint_vm->shownBox();
}

ShowBoxInViewModel::ShowBoxInViewModel(const AbstractConstraintViewModel *constraint_vm,
                                       id_type<BoxModel> boxId) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_constraintViewModelPath {iscore::IDocument::path(constraint_vm) },
m_boxId {boxId}
{
    m_previousBoxId = constraint_vm->shownBox();
}

void ShowBoxInViewModel::undo()
{
    auto constraint_vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();

    if(m_previousBoxId.val())
    {
        constraint_vm->showBox(m_previousBoxId);
    }
    else
    {
        constraint_vm->hideBox();
    }
}

void ShowBoxInViewModel::redo()
{
    auto constraint_vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();
    constraint_vm->showBox(m_boxId);
}

int ShowBoxInViewModel::id() const
{
    return 1;
}

bool ShowBoxInViewModel::mergeWith(const QUndoCommand* other)
{
    return false;
}

void ShowBoxInViewModel::serializeImpl(QDataStream& s) const
{
    s << m_constraintViewModelPath
      << m_boxId
      << m_previousBoxId;
}

void ShowBoxInViewModel::deserializeImpl(QDataStream& s)
{
    s >> m_constraintViewModelPath
      >> m_boxId
      >> m_previousBoxId;
}

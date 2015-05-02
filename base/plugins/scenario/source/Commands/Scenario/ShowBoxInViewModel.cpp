#include "ShowBoxInViewModel.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"
#include "iscore/document/DocumentInterface.hpp"

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
    auto& vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();
    m_previousBoxId = vm.shownBox();
}

ShowBoxInViewModel::ShowBoxInViewModel(const AbstractConstraintViewModel *vm,
                                       id_type<BoxModel> boxId) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_constraintViewModelPath {iscore::IDocument::path(vm) },
    m_boxId {boxId}
{
    m_previousBoxId = vm->shownBox();
}

void ShowBoxInViewModel::undo()
{
    auto& vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();

    if(m_previousBoxId.val())
    {
        vm.showBox(m_previousBoxId);
    }
    else
    {
        vm.hideBox();
    }
}

void ShowBoxInViewModel::redo()
{
    auto& vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();
    vm.showBox(m_boxId);
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

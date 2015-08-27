#include "ShowRackInViewModel.hpp"
#include "Document/Constraint/ViewModels/ConstraintViewModel.hpp"
#include "iscore/document/DocumentInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

ShowRackInViewModel::ShowRackInViewModel(
        Path<ConstraintViewModel>&& constraint_path,
        Id<RackModel> rackId) :
    SerializableCommand{"ScenarioControl",
                        commandName(),
                        description()},
    m_constraintViewPath {std::move(constraint_path) },
    m_rackId {rackId}
{
    auto& vm = m_constraintViewPath.find();
    m_previousRackId = vm.shownRack();
}

ShowRackInViewModel::ShowRackInViewModel(
        const ConstraintViewModel& vm,
        const Id<RackModel>& rackId) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_constraintViewPath {iscore::IDocument::path(vm)},
    m_rackId {rackId}
{
    m_previousRackId = vm.shownRack();
}

void ShowRackInViewModel::undo()
{
    auto& vm = m_constraintViewPath.find();

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
    auto& vm = m_constraintViewPath.find();
    vm.showRack(m_rackId);
}

void ShowRackInViewModel::serializeImpl(QDataStream& s) const
{
    s << m_constraintViewPath
      << m_rackId
      << m_previousRackId;
}

void ShowRackInViewModel::deserializeImpl(QDataStream& s)
{
    s >> m_constraintViewPath
            >> m_rackId
            >> m_previousRackId;
}

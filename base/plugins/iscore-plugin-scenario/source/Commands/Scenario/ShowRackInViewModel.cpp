#include "ShowRackInViewModel.hpp"
#include "Document/Constraint/ViewModels/ConstraintViewModel.hpp"
#include "iscore/document/DocumentInterface.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>

using namespace iscore;
using namespace Scenario::Command;

ShowRackInViewModel::ShowRackInViewModel(
        Path<ConstraintViewModel>&& constraint_path,
        Id<RackModel> rackId) :
    SerializableCommand{factoryName(),
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
    SerializableCommand {factoryName(),
                         commandName(),
                         description()},
    m_constraintViewPath {vm},
    m_rackId {rackId}
{
    m_previousRackId = vm.shownRack();
}

void ShowRackInViewModel::undo() const
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

void ShowRackInViewModel::redo() const
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

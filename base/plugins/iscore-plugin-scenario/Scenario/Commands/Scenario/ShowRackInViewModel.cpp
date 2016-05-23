#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <algorithm>

#include "ShowRackInViewModel.hpp"
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

namespace Scenario
{
namespace Command
{
ShowRackInViewModel::ShowRackInViewModel(
        Path<ConstraintViewModel>&& constraint_path,
        Id<RackModel> rackId) :
    m_constraintViewPath {std::move(constraint_path) },
    m_rackId {std::move(rackId)}
{
    auto& vm = m_constraintViewPath.find();
    m_previousRackId = vm.shownRack();
}

ShowRackInViewModel::ShowRackInViewModel(
        const ConstraintViewModel& vm,
        Id<RackModel> rackId) :
    m_constraintViewPath {vm},
    m_rackId {std::move(rackId)}
{
    m_previousRackId = vm.shownRack();
}

void ShowRackInViewModel::undo() const
{
    auto& vm = m_constraintViewPath.find();

    // TODO unnecessary
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

void ShowRackInViewModel::serializeImpl(DataStreamInput& s) const
{
    s << m_constraintViewPath
      << m_rackId
      << m_previousRackId;
}

void ShowRackInViewModel::deserializeImpl(DataStreamOutput& s)
{
    s >> m_constraintViewPath
            >> m_rackId
            >> m_previousRackId;
}
}
}

#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <boost/core/explicit_operator_bool.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <algorithm>

#include "HideRackInViewModel.hpp"
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

using namespace iscore;
using namespace Scenario::Command;

HideRackInViewModel::HideRackInViewModel(
        Path<ConstraintViewModel>&& path) :
    m_constraintViewPath {std::move(path) }
{
    auto& constraint_vm = m_constraintViewPath.find();
    m_constraintPreviousId = constraint_vm.shownRack();
}

HideRackInViewModel::HideRackInViewModel(
        const ConstraintViewModel& constraint_vm) :
    m_constraintViewPath {constraint_vm}
{
    m_constraintPreviousId = constraint_vm.shownRack();
}

void HideRackInViewModel::undo() const
{
    auto& vm = m_constraintViewPath.find();
    vm.showRack(m_constraintPreviousId);
}

void HideRackInViewModel::redo() const
{
    auto& vm = m_constraintViewPath.find();
    vm.hideRack();
}

void HideRackInViewModel::serializeImpl(DataStreamInput& s) const
{
    s << m_constraintViewPath
      << m_constraintPreviousId;
}

void HideRackInViewModel::deserializeImpl(DataStreamOutput& s)
{
    s >> m_constraintViewPath
            >> m_constraintPreviousId;
}

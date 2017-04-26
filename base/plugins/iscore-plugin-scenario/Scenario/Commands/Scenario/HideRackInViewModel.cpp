#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <algorithm>
#include <iscore/serialization/DataStreamVisitor.hpp>

#include "HideRackInViewModel.hpp"
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
HideRack::HideRack(
    const Scenario::ConstraintModel& constraint_vm)
    : m_constraintViewPath{constraint_vm}
{
}

void HideRack::undo() const
{
  auto& vm = m_constraintViewPath.find();
  vm.setSmallViewVisible(true);
}

void HideRack::redo() const
{
  auto& vm = m_constraintViewPath.find();
  vm.setSmallViewVisible(false);
}

void HideRack::serializeImpl(DataStreamInput& s) const
{
  s << m_constraintViewPath;
}

void HideRack::deserializeImpl(DataStreamOutput& s)
{
  s >> m_constraintViewPath;
}
}
}

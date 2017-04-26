#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <algorithm>
#include <iscore/serialization/DataStreamVisitor.hpp>

#include "ShowRackInViewModel.hpp"
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
ShowRack::ShowRack(
    const ConstraintModel& vm)
    : m_constraintViewPath{vm}
{
}


void ShowRack::undo() const
{
  auto& vm = m_constraintViewPath.find();
  vm.setSmallViewVisible(false);
}

void ShowRack::redo() const
{
  auto& vm = m_constraintViewPath.find();
  vm.setSmallViewVisible(true);
}

void ShowRack::serializeImpl(DataStreamInput& s) const
{
  s << m_constraintViewPath;
}

void ShowRack::deserializeImpl(DataStreamOutput& s)
{
  s >> m_constraintViewPath;
}
}
}

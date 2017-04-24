#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <algorithm>

#include "SwapSlots.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
SwapSlots::SwapSlots(
    Path<ConstraintModel>&& rack, int first, int second)
    : m_path{std::move(rack)}
    , m_first{std::move(first)}
    , m_second{std::move(second)}
{
}

void SwapSlots::undo() const
{
  redo();
}

void SwapSlots::redo() const
{
  auto& cst = m_path.find();
  cst.swapSlots(m_first, m_second, false);
}

void SwapSlots::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_first << m_second;
}

void SwapSlots::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_first >> m_second;
}
}
}



#include "ResizeSlotVertically.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{

ResizeSlotVertically::ResizeSlotVertically(
    const SlotPath& slotPath,
    double newSize)
  : m_path{slotPath}, m_newSize{newSize}
{
  auto& cst = m_path.constraint.find();
  m_originalSize = cst.getSlotHeight(m_path);
}

ResizeSlotVertically::ResizeSlotVertically(
    SlotPath&& slotPath, double newSize)
    : m_path{slotPath}, m_newSize{newSize}
{
  auto& cst = m_path.constraint.find();
  m_originalSize = cst.getSlotHeight(m_path);
}

void ResizeSlotVertically::undo() const
{
  auto& cst = m_path.constraint.find();
  cst.setSlotHeight(m_path, m_originalSize);
}

void ResizeSlotVertically::redo() const
{
  auto& cst = m_path.constraint.find();
  cst.setSlotHeight(m_path, m_newSize);
}

void ResizeSlotVertically::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_originalSize << m_newSize;
}

// Would be better in a ctor ?
void ResizeSlotVertically::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_originalSize >> m_newSize;
}
}
}

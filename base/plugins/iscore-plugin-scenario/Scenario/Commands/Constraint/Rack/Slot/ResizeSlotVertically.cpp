#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

#include "ResizeSlotVertically.hpp"
#include "iscore/serialization/DataStreamVisitor.hpp"
#include "iscore/tools/ModelPath.hpp"
#include "iscore/tools/ModelPathSerialization.hpp"

using namespace iscore;
using namespace Scenario::Command;

ResizeSlotVertically::ResizeSlotVertically(
        Path<SlotModel>&& slotPath,
        double newSize) :
    m_path {slotPath},
    m_newSize {newSize}
{
    auto& slot = m_path.find();
    m_originalSize = slot.height();
}

void ResizeSlotVertically::undo() const
{
    auto& slot = m_path.find();
    slot.setHeight(m_originalSize);
}

void ResizeSlotVertically::redo() const
{
    auto& slot = m_path.find();
    slot.setHeight(m_newSize);
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

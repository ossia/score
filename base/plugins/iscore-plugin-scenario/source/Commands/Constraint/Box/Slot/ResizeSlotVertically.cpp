#include "ResizeSlotVertically.hpp"

#include "Document/Constraint/Box/Slot/SlotModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

ResizeSlotVertically::ResizeSlotVertically(ObjectPath&& slotPath,
                                           double newSize) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {slotPath},
    m_newSize {newSize}
{
    auto& slot = m_path.find<SlotModel>();
    m_originalSize = slot.height();
}

void ResizeSlotVertically::undo()
{
    auto& slot = m_path.find<SlotModel>();
    slot.setHeight(m_originalSize);
}

void ResizeSlotVertically::redo()
{
    auto& slot = m_path.find<SlotModel>();
    slot.setHeight(m_newSize);
}



void ResizeSlotVertically::serializeImpl(QDataStream& s) const
{
    s << m_path << m_originalSize << m_newSize;
}

// Would be better in a ctor ?
void ResizeSlotVertically::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_originalSize >> m_newSize;
}

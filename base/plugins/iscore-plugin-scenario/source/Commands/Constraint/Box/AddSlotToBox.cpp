#include "AddSlotToBox.hpp"

#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Slot/SlotModel.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace iscore;
using namespace Scenario::Command;

AddSlotToBox::AddSlotToBox(ObjectPath&& boxPath) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {boxPath}
{
    auto& box = m_path.find<BoxModel>();
    m_createdSlotId = getStrongId(box.getSlots());
}

void AddSlotToBox::undo()
{
    auto& box = m_path.find<BoxModel>();
    box.removeSlot(m_createdSlotId);
}

void AddSlotToBox::redo()
{
    auto& box = m_path.find<BoxModel>();
    box.addSlot(new SlotModel {m_createdSlotId,
                               &box});
}

void AddSlotToBox::serializeImpl(QDataStream& s) const
{
    s << m_path << m_createdSlotId;
}

void AddSlotToBox::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_createdSlotId;
}

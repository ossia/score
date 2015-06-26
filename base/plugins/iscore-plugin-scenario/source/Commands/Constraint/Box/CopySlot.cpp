#include "CopySlot.hpp"

#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Slot/SlotModel.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace iscore;
using namespace Scenario::Command;

CopySlot::CopySlot(ObjectPath&& slotToCopy,
                   ObjectPath&& targetBoxPath) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_slotPath {slotToCopy},
    m_targetBoxPath {targetBoxPath}
{
    auto& box = m_targetBoxPath.find<BoxModel>();
    m_newSlotId = getStrongId(box.getSlots());
}

void CopySlot::undo()
{
    auto& targetBox = m_targetBoxPath.find<BoxModel>();
    targetBox.removeSlot(m_newSlotId);
}


void CopySlot::redo()
{
    auto& sourceSlot = m_slotPath.find<SlotModel>();
    auto& targetBox = m_targetBoxPath.find<BoxModel>();

    targetBox.addSlot(new SlotModel {&SlotModel::copyViewModelsInSameConstraint,
                                      sourceSlot,
                                      m_newSlotId,
                                      &targetBox});
}

void CopySlot::serializeImpl(QDataStream& s) const
{
    s << m_slotPath << m_targetBoxPath << m_newSlotId;
}

void CopySlot::deserializeImpl(QDataStream& s)
{
    s >> m_slotPath >> m_targetBoxPath >> m_newSlotId;
}

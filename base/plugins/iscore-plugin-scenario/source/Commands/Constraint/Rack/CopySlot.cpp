#include "CopySlot.hpp"

#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace iscore;
using namespace Scenario::Command;

CopySlot::CopySlot(ObjectPath&& slotToCopy,
                   ObjectPath&& targetRackPath) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_slotPath {slotToCopy},
    m_targetRackPath {targetRackPath}
{
    auto& rack = m_targetRackPath.find<RackModel>();
    m_newSlotId = getStrongId(rack.getSlots());
}

void CopySlot::undo()
{
    auto& targetRack = m_targetRackPath.find<RackModel>();
    targetRack.removeSlot(m_newSlotId);
}


void CopySlot::redo()
{
    auto& sourceSlot = m_slotPath.find<SlotModel>();
    auto& targetRack = m_targetRackPath.find<RackModel>();

    targetRack.addSlot(new SlotModel {&SlotModel::copyViewModelsInSameConstraint,
                                      sourceSlot,
                                      m_newSlotId,
                                      &targetRack});
}

void CopySlot::serializeImpl(QDataStream& s) const
{
    s << m_slotPath << m_targetRackPath << m_newSlotId;
}

void CopySlot::deserializeImpl(QDataStream& s)
{
    s >> m_slotPath >> m_targetRackPath >> m_newSlotId;
}

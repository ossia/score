#include "CopySlot.hpp"

#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace iscore;
using namespace Scenario::Command;

CopySlot::CopySlot(Path<SlotModel>&& slotToCopy,
                   Path<RackModel>&& targetRackPath) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_slotPath {slotToCopy},
    m_targetRackPath {targetRackPath}
{
    auto& rack = m_targetRackPath.find();
    m_newSlotId = getStrongId(rack.slotmodels);
}

void CopySlot::undo()
{
    auto& targetRack = m_targetRackPath.find();
    targetRack.slotmodels.remove(m_newSlotId);
}


void CopySlot::redo()
{
    const auto& sourceSlot = m_slotPath.find();
    auto& targetRack = m_targetRackPath.find();

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

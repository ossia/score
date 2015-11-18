#include "CopySlot.hpp"

#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace iscore;
using namespace Scenario::Command;

CopySlot::CopySlot(Path<SlotModel>&& slotToCopy,
                   Path<RackModel>&& targetRackPath) :
    m_slotPath {slotToCopy},
    m_targetRackPath {targetRackPath}
{
    auto& rack = m_targetRackPath.find();
    m_newSlotId = getStrongId(rack.slotmodels);
}

void CopySlot::undo() const
{
    auto& targetRack = m_targetRackPath.find();
    targetRack.slotmodels.remove(m_newSlotId);
}


void CopySlot::redo() const
{
    const auto& sourceSlot = m_slotPath.find();
    auto& targetRack = m_targetRackPath.find();

    targetRack.addSlot(new SlotModel {&SlotModel::copyViewModelsInSameConstraint,
                                      sourceSlot,
                                      m_newSlotId,
                                      &targetRack});
}

void CopySlot::serializeImpl(DataStreamInput& s) const
{
    s << m_slotPath << m_targetRackPath << m_newSlotId;
}

void CopySlot::deserializeImpl(DataStreamOutput& s)
{
    s >> m_slotPath >> m_targetRackPath >> m_newSlotId;
}

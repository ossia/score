#include "AddSlotToRack.hpp"

#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace iscore;
using namespace Scenario::Command;

AddSlotToRack::AddSlotToRack(Path<RackModel>&& rackPath) :
    m_path {rackPath}
{
    auto rack = m_path.try_find(); // Because we use this in a macro, the rack may not be there yet

    if(rack)
        m_createdSlotId = getStrongId(rack->slotmodels);
    else
        m_createdSlotId = Id<SlotModel>{iscore::id_generator::getFirstId()};
}

void AddSlotToRack::undo() const
{
    auto& rack = m_path.find();
    rack.slotmodels.remove(m_createdSlotId);
}

void AddSlotToRack::redo() const
{
    auto& rack = m_path.find();
    rack.addSlot(new SlotModel {m_createdSlotId,
                               &rack});
}

void AddSlotToRack::serializeImpl(QDataStream& s) const
{
    s << m_path << m_createdSlotId;
}

void AddSlotToRack::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_createdSlotId;
}

#include "RemoveSlotFromRack.hpp"

#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

using namespace iscore;
using namespace Scenario::Command;

RemoveSlotFromRack::RemoveSlotFromRack(Path<SlotModel>&& slotPath)
{
    auto rackPath = slotPath.unsafePath().vec();
    auto lastId = rackPath.takeLast();
    m_path = Path<RackModel>{
            ObjectPath{std::move(rackPath)},
            Path<RackModel>::UnsafeDynamicCreation{}};
    m_slotId = Id<SlotModel> (lastId.id());

    auto& rack = m_path.find();
    m_position = rack.slotPosition(m_slotId);

    Serializer<DataStream> s{&m_serializedSlotData};
    s.readFrom(rack.slotmodels.at(m_slotId));
}

RemoveSlotFromRack::RemoveSlotFromRack(
        Path<RackModel>&& rackPath,
        Id<SlotModel> slotId) :
    m_path {rackPath},
    m_slotId {slotId}
{
    auto& rack = m_path.find();
    Serializer<DataStream> s{&m_serializedSlotData};

    s.readFrom(rack.slotmodels.at(slotId));
    m_position = rack.slotPosition(slotId);
}

void RemoveSlotFromRack::undo() const
{
    auto& rack = m_path.find();
    Deserializer<DataStream> s {m_serializedSlotData};
    rack.addSlot(new SlotModel {s, &rack}, m_position);
}

void RemoveSlotFromRack::redo() const
{
    auto& rack = m_path.find();
    rack.slotmodels.remove(m_slotId);
}

void RemoveSlotFromRack::serializeImpl(QDataStream& s) const
{
    s << m_path << m_slotId << m_position << m_serializedSlotData;
}

void RemoveSlotFromRack::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_slotId >> m_position >> m_serializedSlotData;
}

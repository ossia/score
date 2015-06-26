#include "RemoveSlotFromRack.hpp"

#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveSlotFromRack::RemoveSlotFromRack(ObjectPath&& slotPath) :
    SerializableCommand {"ScenarioControl", commandName(), description()}
{
    auto rackPath = slotPath.vec();
    auto lastId = rackPath.takeLast();
    m_path = ObjectPath{std::move(rackPath) };
    m_slotId = id_type<SlotModel> (lastId.id());

    auto& rack = m_path.find<RackModel>();
    m_position = rack.slotPosition(m_slotId);

    Serializer<DataStream> s{&m_serializedSlotData};
    s.readFrom(*rack.slot(m_slotId));
}

RemoveSlotFromRack::RemoveSlotFromRack(ObjectPath&& rackPath, id_type<SlotModel> slotId) :
    SerializableCommand {"ScenarioControl", commandName(), description()},
    m_path {rackPath},
    m_slotId {slotId}
{
    auto& rack = m_path.find<RackModel>();
    Serializer<DataStream> s{&m_serializedSlotData};

    s.readFrom(*rack.slot(slotId));
    m_position = rack.slotPosition(slotId);
}

void RemoveSlotFromRack::undo()
{
    auto& rack = m_path.find<RackModel>();
    Deserializer<DataStream> s {m_serializedSlotData};
    rack.addSlot(new SlotModel {s, &rack}, m_position);
}

void RemoveSlotFromRack::redo()
{
    auto& rack = m_path.find<RackModel>();
    rack.removeSlot(m_slotId);
}

void RemoveSlotFromRack::serializeImpl(QDataStream& s) const
{
    s << m_path << m_slotId << m_position << m_serializedSlotData;
}

void RemoveSlotFromRack::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_slotId >> m_position >> m_serializedSlotData;
}

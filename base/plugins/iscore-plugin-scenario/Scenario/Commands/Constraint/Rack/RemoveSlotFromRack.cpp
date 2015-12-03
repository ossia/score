#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

#include <type_traits>
#include <utility>

#include "RemoveSlotFromRack.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/ObjectIdentifier.hpp>

using namespace iscore;
using namespace Scenario::Command;

RemoveSlotFromRack::RemoveSlotFromRack(Path<SlotModel>&& slotPath)
{
    auto trimmedSlotPath = std::move(slotPath).splitLast<RackModel>();

    m_path = std::move(trimmedSlotPath.first);
    m_slotId = Id<SlotModel>{trimmedSlotPath.second.id()};

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

void RemoveSlotFromRack::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_slotId << m_position << m_serializedSlotData;
}

void RemoveSlotFromRack::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_slotId >> m_position >> m_serializedSlotData;
}

#include "RemoveSlotFromBox.hpp"

#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Slot/SlotModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveSlotFromBox::RemoveSlotFromBox(ObjectPath&& slotPath) :
    SerializableCommand {"ScenarioControl", commandName(), description()}
{
    auto boxPath = slotPath.vec();
    auto lastId = boxPath.takeLast();
    m_path = ObjectPath{std::move(boxPath) };
    m_slotId = id_type<SlotModel> (lastId.id());

    auto& box = m_path.find<BoxModel>();
    m_position = box.slotPosition(m_slotId);

    Serializer<DataStream> s{&m_serializedSlotData};
    s.readFrom(*box.slot(m_slotId));
}

RemoveSlotFromBox::RemoveSlotFromBox(ObjectPath&& boxPath, id_type<SlotModel> slotId) :
    SerializableCommand {"ScenarioControl", commandName(), description()},
    m_path {boxPath},
    m_slotId {slotId}
{
    auto& box = m_path.find<BoxModel>();
    Serializer<DataStream> s{&m_serializedSlotData};

    s.readFrom(*box.slot(slotId));
    m_position = box.slotPosition(slotId);
}

void RemoveSlotFromBox::undo()
{
    auto& box = m_path.find<BoxModel>();
    Deserializer<DataStream> s {m_serializedSlotData};
    box.addSlot(new SlotModel {s, &box}, m_position);
}

void RemoveSlotFromBox::redo()
{
    auto& box = m_path.find<BoxModel>();
    box.removeSlot(m_slotId);
}

void RemoveSlotFromBox::serializeImpl(QDataStream& s) const
{
    s << m_path << m_slotId << m_position << m_serializedSlotData;
}

void RemoveSlotFromBox::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_slotId >> m_position >> m_serializedSlotData;
}

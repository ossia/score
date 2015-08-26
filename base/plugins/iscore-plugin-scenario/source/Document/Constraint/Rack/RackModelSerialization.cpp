#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "RackModel.hpp"
#include "Slot/SlotModel.hpp"

template<> void Visitor<Reader<DataStream>>::readFrom(const RackModel& rack)
{
    readFrom(static_cast<const IdentifiedObject<RackModel>&>(rack));

    m_stream << rack.slotsPositions();

    auto theSlots = rack.getSlots();
    m_stream << (int) theSlots.size();

    for(const auto& slot : theSlots)
    {
        readFrom(slot);
    }

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(RackModel& rack)
{
    int slots_size;
    QList<id_type<SlotModel>> positions;
    m_stream >> positions;

    m_stream >> slots_size;

    for(; slots_size -- > 0 ;)
    {
        auto slot = new SlotModel(*this, &rack);
        rack.addSlot(slot, positions.indexOf(slot->id()));
    }

    checkDelimiter();
}


template<> void Visitor<Reader<JSONObject>>::readFrom(const RackModel& rack)
{
    readFrom(static_cast<const IdentifiedObject<RackModel>&>(rack));

    QJsonArray arr;
    for(const auto& slot : rack.getSlots())
    {
        arr.push_back(toJsonObject(slot));
    }

    m_obj["Slots"] = arr;

    QJsonArray positions;

    for(auto& id : rack.slotsPositions())
    {
        positions.append(*id.val());
    }

    m_obj["SlotsPositions"] = positions;
}

template<> void Visitor<Writer<JSONObject>>::writeTo(RackModel& rack)
{
    QJsonArray theSlots = m_obj["Slots"].toArray();
    QJsonArray slotsPositions = m_obj["SlotsPositions"].toArray();
    QList<id_type<SlotModel>> list;

    for(auto elt : slotsPositions)
    {
        list.push_back(id_type<SlotModel> {elt.toInt() });
    }

    for(const auto& json_slot : theSlots)
    {
        Deserializer<JSONObject> deserializer {json_slot.toObject()};
        auto slot = new SlotModel {deserializer, &rack};
        rack.addSlot(slot, list.indexOf(slot->id()));
    }
}

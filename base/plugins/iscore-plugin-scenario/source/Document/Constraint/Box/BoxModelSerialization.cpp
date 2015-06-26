#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "BoxModel.hpp"
#include "Slot/SlotModel.hpp"

template<> void Visitor<Reader<DataStream>>::readFrom(const BoxModel& box)
{
    readFrom(static_cast<const IdentifiedObject<BoxModel>&>(box));

    readFrom(box.metadata);
    m_stream << box.slotsPositions();

    auto theSlots = box.getSlots();
    m_stream << (int) theSlots.size();

    for(auto slot : theSlots)
    {
        readFrom(*slot);
    }

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(BoxModel& box)
{
    int slots_size;
    QList<id_type<SlotModel>> positions;
    writeTo(box.metadata);
    m_stream >> positions;

    m_stream >> slots_size;

    for(; slots_size -- > 0 ;)
    {
        auto slot = new SlotModel(*this, &box);
        box.addSlot(slot, positions.indexOf(slot->id()));
    }

    checkDelimiter();
}


template<> void Visitor<Reader<JSONObject>>::readFrom(const BoxModel& box)
{
    readFrom(static_cast<const IdentifiedObject<BoxModel>&>(box));

    m_obj["Metadata"] = toJsonObject(box.metadata);

    QJsonArray arr;
    for(auto slot : box.getSlots())
    {
        arr.push_back(toJsonObject(*slot));
    }

    m_obj["Slots"] = arr;

    QJsonArray positions;

    for(auto& id : box.slotsPositions())
    {
        positions.append(*id.val());
    }

    m_obj["SlotsPositions"] = positions;
}

template<> void Visitor<Writer<JSONObject>>::writeTo(BoxModel& box)
{
    box.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"].toObject());

    QJsonArray theSlots = m_obj["Slots"].toArray();
    QJsonArray slotsPositions = m_obj["SlotsPositions"].toArray();
    QList<id_type<SlotModel>> list;

    for(auto elt : slotsPositions)
    {
        list.push_back(id_type<SlotModel> {elt.toInt() });
    }

    for(int i = 0; i < theSlots.size(); i++)
    {
        Deserializer<JSONObject> deserializer {theSlots[i].toObject() };
        auto slot = new SlotModel {deserializer, &box};
        box.addSlot(slot, list.indexOf(slot->id()));
    }
}

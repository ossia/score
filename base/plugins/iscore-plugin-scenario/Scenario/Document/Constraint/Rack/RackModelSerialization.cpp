
#include <boost/optional/optional.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <sys/types.h>

#include "RackModel.hpp"
#include "Slot/SlotModel.hpp"
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

template <typename T> class Reader;
template <typename T> class Writer;
template <typename model> class IdentifiedObject;

template<> void Visitor<Reader<DataStream>>::readFrom(const RackModel& rack)
{
    readFrom(static_cast<const IdentifiedObject<RackModel>&>(rack));

    m_stream << rack.slotsPositions();

    const auto& theSlots = rack.slotmodels;
    m_stream << (int32_t) theSlots.size();

    for(const auto& slot : theSlots)
    {
        readFrom(slot);
    }

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(RackModel& rack)
{
    int32_t slots_size;
    QList<Id<SlotModel>> positions;
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
    for(const auto& slot : rack.slotmodels)
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
    QList<Id<SlotModel>> list;

    for(auto elt : slotsPositions)
    {
        list.push_back(Id<SlotModel> {elt.toInt() });
    }

    for(const auto& json_slot : theSlots)
    {
        Deserializer<JSONObject> deserializer {json_slot.toObject()};
        auto slot = new SlotModel {deserializer, &rack};
        rack.addSlot(slot, list.indexOf(slot->id()));
    }
}

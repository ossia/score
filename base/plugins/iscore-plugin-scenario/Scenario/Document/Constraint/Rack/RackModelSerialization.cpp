
#include <QDataStream>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QtGlobal>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <sys/types.h>

#include "RackModel.hpp"
#include "Slot/SlotModel.hpp"
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/Identifier.hpp>

template <typename T>
class Reader;
template <typename T>
class Writer;
template <typename model>
class IdentifiedObject;


template <>
void DataStreamReader::read(const Scenario::RackModel& rack)
{
  m_stream << rack.slotsPositions();

  const auto& theSlots = rack.slotmodels;
  m_stream << (int32_t)theSlots.size();

  for (const auto& slot : theSlots)
  {
    readFrom(slot);
  }

  insertDelimiter();
}


template <>
void DataStreamWriter::writeTo(Scenario::RackModel& rack)
{
  int32_t slots_size;
  QList<Id<Scenario::SlotModel>> positions;
  m_stream >> positions;

  m_stream >> slots_size;

  for (; slots_size-- > 0;)
  {
    auto slot = new Scenario::SlotModel(*this, &rack);
    rack.addSlot(slot, positions.indexOf(slot->id()));
  }

  checkDelimiter();
}


template <>
void JSONObjectReader::read(const Scenario::RackModel& rack)
{
  QJsonArray arr;
  for (const auto& slot : rack.slotmodels)
  {
    arr.push_back(toJsonObject(slot));
  }

  obj["Slots"] = arr;

  QJsonArray positions;

  for (auto& id : rack.slotsPositions())
  {
    positions.append(id.val());
  }

  obj["SlotsPositions"] = positions;
}


template <>
void JSONObjectWriter::writeTo(Scenario::RackModel& rack)
{
  QJsonArray theSlots = obj["Slots"].toArray();
  QJsonArray slotsPositions = obj["SlotsPositions"].toArray();
  QMap<Id<Scenario::SlotModel>, int> list;

  int i = 0;
  for (auto elt : slotsPositions)
  {
    list.insert(Id<Scenario::SlotModel>{elt.toInt()}, i);
    i++;
  }

  for (const auto& json_slot : theSlots)
  {
    JSONObject::Deserializer deserializer{json_slot.toObject()};
    auto slot = new Scenario::SlotModel{deserializer, &rack};
    rack.addSlot(slot, list[slot->id()]);
  }
}

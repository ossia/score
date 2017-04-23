
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
  const auto& theSlots = rack.slotmodels;
  m_stream << (int32_t)theSlots.size();

  for (const auto& slot : theSlots)
  {
    readFrom(slot);
  }

  insertDelimiter();
}


template <>
void DataStreamWriter::write(Scenario::RackModel& rack)
{
  rack.slotmodels.clear();

  int32_t slots_size;
  m_stream >> slots_size;

  int i = 0;
  for (; slots_size-- > 0;)
  {
    auto slot = new Scenario::SlotModel(*this, &rack);
    rack.addSlot(slot, i++);
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
}


template <>
void JSONObjectWriter::write(Scenario::RackModel& rack)
{
  rack.slotmodels.clear();

  QJsonArray theSlots = obj["Slots"].toArray();

  int i = 0;
  for (const auto& json_slot : theSlots)
  {
    JSONObject::Deserializer deserializer{json_slot.toObject()};
    auto slot = new Scenario::SlotModel{deserializer, &rack};
    rack.addSlot(slot, i++);
  }
}

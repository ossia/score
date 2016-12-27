
#include <QDataStream>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QtGlobal>
#include <algorithm>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <Process/TimeValue.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <State/Expression.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/model/tree/TreeNode.hpp>

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
Visitor<Reader<DataStream>>::read(const Scenario::EventModel& ev)
{
  m_stream << ev.m_timeNode << ev.m_states << ev.m_condition << ev.m_extent
           << ev.m_date << ev.m_offset;

  insertDelimiter();
}

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
Visitor<Writer<DataStream>>::writeTo(Scenario::EventModel& ev)
{
  m_stream >> ev.m_timeNode >> ev.m_states >> ev.m_condition >> ev.m_extent
      >> ev.m_date >> ev.m_offset;

  checkDelimiter();
}

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
Visitor<Reader<JSONObject>>::readFrom(const Scenario::EventModel& ev)
{
  readFrom(static_cast<const iscore::Entity<Scenario::EventModel>&>(ev));

  m_obj["TimeNode"] = toJsonValue(ev.m_timeNode);
  m_obj["States"] = toJsonArray(ev.m_states);

  m_obj["Condition"] = toJsonObject(ev.m_condition);

  m_obj["Extent"] = toJsonValue(ev.m_extent);
  m_obj["Date"] = toJsonValue(ev.m_date);
  m_obj["Offset"] = (int32_t)ev.m_offset;
}

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(Scenario::EventModel& ev)
{
  ev.m_timeNode
      = fromJsonValue<Id<Scenario::TimeNodeModel>>(m_obj["TimeNode"]);
  fromJsonValueArray(m_obj["States"].toArray(), ev.m_states);

  fromJsonObject(m_obj["Condition"], ev.m_condition);

  ev.m_extent = fromJsonValue<Scenario::VerticalExtent>(m_obj["Extent"]);
  ev.m_date = fromJsonValue<TimeValue>(m_obj["Date"]);
  ev.m_offset = static_cast<Scenario::OffsetBehavior>(m_obj["Offset"].toInt());
}

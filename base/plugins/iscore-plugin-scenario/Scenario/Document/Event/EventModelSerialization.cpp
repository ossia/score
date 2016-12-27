
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
DataStreamReader::read(const Scenario::EventModel& ev)
{
  m_stream << ev.m_timeNode << ev.m_states << ev.m_condition << ev.m_extent
           << ev.m_date << ev.m_offset;

  insertDelimiter();
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamWriter::writeTo(Scenario::EventModel& ev)
{
  m_stream >> ev.m_timeNode >> ev.m_states >> ev.m_condition >> ev.m_extent
      >> ev.m_date >> ev.m_offset;

  checkDelimiter();
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
JSONObjectReader::read(const Scenario::EventModel& ev)
{
  obj["TimeNode"] = toJsonValue(ev.m_timeNode);
  obj["States"] = toJsonArray(ev.m_states);

  obj["Condition"] = toJsonObject(ev.m_condition);

  obj["Extent"] = toJsonValue(ev.m_extent);
  obj["Date"] = toJsonValue(ev.m_date);
  obj["Offset"] = (int32_t)ev.m_offset;
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
JSONObjectWriter::writeTo(Scenario::EventModel& ev)
{
  ev.m_timeNode
      = fromJsonValue<Id<Scenario::TimeNodeModel>>(obj["TimeNode"]);
  fromJsonValueArray(obj["States"].toArray(), ev.m_states);

  fromJsonObject(obj["Condition"], ev.m_condition);

  ev.m_extent = fromJsonValue<Scenario::VerticalExtent>(obj["Extent"]);
  ev.m_date = fromJsonValue<TimeValue>(obj["Date"]);
  ev.m_offset = static_cast<Scenario::OffsetBehavior>(obj["Offset"].toInt());
}

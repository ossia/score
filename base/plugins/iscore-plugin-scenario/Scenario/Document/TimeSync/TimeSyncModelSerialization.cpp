// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <QDataStream>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QtGlobal>
#include <algorithm>
#include <iscore/tools/std/Optional.hpp>

#include "TimeSyncModel.hpp"
#include <Process/TimeValue.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <State/Expression.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/model/tree/TreeNode.hpp>

template <typename T>
class Reader;
template <typename T>
class Writer;
template <typename model>
class IdentifiedObject;


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamReader::read(const Scenario::TimeSyncModel& timesync)
{
  m_stream << timesync.m_date << timesync.m_events << timesync.m_extent
           << timesync.m_active << timesync.m_expression;

  insertDelimiter();
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamWriter::write(Scenario::TimeSyncModel& timesync)
{
  m_stream >> timesync.m_date >> timesync.m_events >> timesync.m_extent
      >> timesync.m_active >> timesync.m_expression;


  checkDelimiter();
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
JSONObjectReader::read(const Scenario::TimeSyncModel& timesync)
{
  obj[strings.Date] = toJsonValue(timesync.date());
  obj[strings.Events] = toJsonArray(timesync.m_events);
  obj[strings.Extent] = toJsonValue(timesync.m_extent);

  QJsonObject trig;
  trig[strings.Active] = timesync.m_active;
  trig[strings.Expression] = toJsonObject(timesync.m_expression);
  obj[strings.Trigger] = trig;
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
JSONObjectWriter::write(Scenario::TimeSyncModel& timesync)
{
  if (timesync.metadata().getLabel() == QStringLiteral("TimeSync"))
    timesync.metadata().setLabel("");

  timesync.m_date = fromJsonValue<TimeVal>(obj[strings.Date]);
  timesync.m_extent = fromJsonValue<Scenario::VerticalExtent>(obj[strings.Extent]);

  fromJsonValueArray(obj[strings.Events].toArray(), timesync.m_events);

  State::Expression t;
  const auto& trig_obj = obj[strings.Trigger].toObject();
  fromJsonObject(trig_obj[strings.Expression], t);
  timesync.m_expression = std::move(t);
  timesync.m_active = trig_obj[strings.Active].toBool();
}

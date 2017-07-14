// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <QDataStream>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QtGlobal>
#include <algorithm>
#include <iscore/tools/std/Optional.hpp>

#include "TimeNodeModel.hpp"
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
DataStreamReader::read(const Scenario::TimeNodeModel& timenode)
{
  m_stream << timenode.m_date << timenode.m_events << timenode.m_extent
           << timenode.m_active << timenode.m_expression;

  insertDelimiter();
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamWriter::write(Scenario::TimeNodeModel& timenode)
{
  m_stream >> timenode.m_date >> timenode.m_events >> timenode.m_extent
      >> timenode.m_active >> timenode.m_expression;


  checkDelimiter();
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
JSONObjectReader::read(const Scenario::TimeNodeModel& timenode)
{
  obj[strings.Date] = toJsonValue(timenode.date());
  obj[strings.Events] = toJsonArray(timenode.m_events);
  obj[strings.Extent] = toJsonValue(timenode.m_extent);

  QJsonObject trig;
  trig[strings.Active] = timenode.m_active;
  trig[strings.Expression] = toJsonObject(timenode.m_expression);
  obj[strings.Trigger] = trig;
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
JSONObjectWriter::write(Scenario::TimeNodeModel& timenode)
{
  if (timenode.metadata().getLabel() == QStringLiteral("TimeNode"))
    timenode.metadata().setLabel("");

  timenode.m_date = fromJsonValue<TimeVal>(obj[strings.Date]);
  timenode.m_extent = fromJsonValue<Scenario::VerticalExtent>(obj[strings.Extent]);

  fromJsonValueArray(obj[strings.Events].toArray(), timenode.m_events);

  State::Expression t;
  const auto& trig_obj = obj[strings.Trigger].toObject();
  fromJsonObject(trig_obj[strings.Expression], t);
  timenode.m_expression = std::move(t);
  timenode.m_active = trig_obj[strings.Active].toBool();
}

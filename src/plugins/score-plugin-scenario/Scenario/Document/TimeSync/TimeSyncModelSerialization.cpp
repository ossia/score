// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "TimeSyncModel.hpp"

#include <State/Expression.hpp>

#include <Process/TimeValue.hpp>
#include <Process/TimeValueSerialization.hpp>

#include <Scenario/Document/VerticalExtent.hpp>

#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/std/Optional.hpp>

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamReader::read(const Scenario::TimeSyncModel& timesync)
{
  m_stream << timesync.m_date << timesync.m_events << timesync.m_musicalSync
           << timesync.m_active << timesync.m_autotrigger << timesync.m_startPoint
           << timesync.m_expression;

  insertDelimiter();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamWriter::write(Scenario::TimeSyncModel& timesync)
{
  m_stream >> timesync.m_date >> timesync.m_events >> timesync.m_musicalSync
      >> timesync.m_active >> timesync.m_autotrigger >> timesync.m_startPoint
      >> timesync.m_expression;

  checkDelimiter();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
JSONReader::read(const Scenario::TimeSyncModel& timesync)
{
  obj[strings.Date] = timesync.date();
  obj[strings.Events] = timesync.m_events;
  obj["MusicalSync"] = timesync.m_musicalSync;
  obj[strings.AutoTrigger] = timesync.m_autotrigger;
  obj["Start"] = timesync.m_startPoint;
  obj[strings.Active] = timesync.m_active;
  obj[strings.Expression] = timesync.m_expression.toString();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void JSONWriter::write(Scenario::TimeSyncModel& timesync)
{
  timesync.metadata().setLabel(QString{});
  timesync.m_date <<= obj[strings.Date];
  timesync.m_events <<= obj[strings.Events];

  assign_with_default(timesync.m_musicalSync, obj.tryGet("MusicalSync"), 1.0);
  assign_with_default(timesync.m_autotrigger, obj.tryGet("AutoTrigger"), false);
  assign_with_default(timesync.m_startPoint, obj.tryGet("Start"), false);

  if(auto trigger_it = obj.tryGet(strings.Trigger))
  {
    // old format
    auto trigger_obj = trigger_it->toObject();
    auto active_it = trigger_obj.FindMember("Active");
    if(active_it != trigger_obj.MemberEnd())
    {
      timesync.m_active = active_it->value.GetBool();
    }
    auto expr_it = trigger_obj.FindMember("Expression");
    if(expr_it != trigger_obj.MemberEnd())
    {
      timesync.m_expression <<= JsonValue{expr_it->value};
    }
  }
  else
  {
    QString exprstr;
    assign_with_default(exprstr, obj.tryGet(strings.Expression), "");
    if(auto expr = State::parseExpression(exprstr))
      timesync.m_expression = *std::move(expr);

    assign_with_default(timesync.m_active, obj.tryGet("Active"), false);
  }
}

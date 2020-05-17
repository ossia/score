// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Process/TimeValue.hpp>
#include <Process/TimeValueSerialization.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <State/Expression.hpp>

#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>



template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamReader::read(const Scenario::EventModel& ev)
{
  m_stream << ev.m_timeSync << ev.m_states << ev.m_condition
           << ev.m_date << ev.m_offset;

  insertDelimiter();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamWriter::write(Scenario::EventModel& ev)
{
  m_stream >> ev.m_timeSync >> ev.m_states >> ev.m_condition
      >> ev.m_date >> ev.m_offset;

  checkDelimiter();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
JSONReader::read(const Scenario::EventModel& ev)
{
  obj[strings.TimeSync] = ev.m_timeSync;
  obj[strings.States] = ev.m_states;

  obj[strings.Condition] = ev.m_condition;

  obj[strings.Date] = ev.m_date;
  obj[strings.Offset] = (int32_t)ev.m_offset;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
JSONWriter::write(Scenario::EventModel& ev)
{
  ev.m_timeSync
      <<= obj[strings.TimeSync];
  ev.m_states <<= obj[strings.States];

  ev.m_condition <<= obj[strings.Condition];

  ev.m_date <<= obj[strings.Date];
  ev.m_offset
      = static_cast<Scenario::OffsetBehavior>(obj[strings.Offset].toInt());
}

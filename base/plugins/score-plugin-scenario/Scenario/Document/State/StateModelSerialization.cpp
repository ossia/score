// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/State/StateModel.hpp>
#include <Process/Process.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <Process/ProcessList.hpp>

#include <Process/State/MessageNode.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <score/model/ModelMetadata.hpp>

#include <score/serialization/JSONValueVisitor.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/Todo.hpp>
#include <score/model/tree/TreeNode.hpp>

namespace Scenario
{
class IntervalModel;
class EventModel;
}
template <typename T>
class Reader;
template <typename T>
class Writer;
template <typename model>
class IdentifiedObject;


template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamReader::read(const Scenario::StateModel& s)
{
  m_stream << s.m_eventId << s.m_previousInterval << s.m_nextInterval
           << s.m_heightPercentage;

  // Message tree
  m_stream << s.m_messageItemModel->rootNode();

  // Processes plugins
  m_stream << (int32_t)s.stateProcesses.size();
  for (const auto& process : s.stateProcesses)
  {
    readFrom(process);
  }

  insertDelimiter();
}


template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamWriter::write(Scenario::StateModel& s)
{
  m_stream >> s.m_eventId >> s.m_previousInterval >> s.m_nextInterval
      >> s.m_heightPercentage;

  // Message tree
  Process::MessageNode n;
  m_stream >> n;
  s.m_messageItemModel = new Scenario::MessageItemModel{s.m_stack, s, &s};
  s.messages() = n;

  // Processes plugins
  int32_t process_count;
  m_stream >> process_count;
  auto& pl = components.interfaces<Process::ProcessFactoryList>();
  for (; process_count-- > 0;)
  {
    auto proc = deserialize_interface(pl, *this, &s);
    if (proc)
      s.stateProcesses.add(proc);
    else
      SCORE_TODO;
  }

  checkDelimiter();
}


template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
JSONObjectReader::read(const Scenario::StateModel& s)
{
  obj[strings.Event] = toJsonValue(s.m_eventId);
  obj[strings.PreviousInterval] = toJsonValue(s.m_previousInterval);
  obj[strings.NextInterval] = toJsonValue(s.m_nextInterval);
  obj[strings.HeightPercentage] = s.m_heightPercentage;

  // Message tree
  obj[strings.Messages] = toJsonObject(s.m_messageItemModel->rootNode());

  // Processes plugins
  obj[strings.StateProcesses] = toJsonArray(s.stateProcesses);
}


template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
JSONObjectWriter::write(Scenario::StateModel& s)
{
  s.m_eventId = fromJsonValue<Id<Scenario::EventModel>>(obj[strings.Event]);
  s.m_previousInterval
      = fromJsonValue<OptionalId<Scenario::IntervalModel>>(
          obj[strings.PreviousInterval]);
  s.m_nextInterval = fromJsonValue<OptionalId<Scenario::IntervalModel>>(
      obj[strings.NextInterval]);
  s.m_heightPercentage = obj[strings.HeightPercentage].toDouble();

  // Message tree
  s.m_messageItemModel = new Scenario::MessageItemModel{s.m_stack, s, &s};
  s.messages() = fromJsonObject<Process::MessageNode>(obj[strings.Messages]);

  // Processes plugins
  auto& pl = components.interfaces<Process::ProcessFactoryList>();

  QJsonArray process_array = obj[strings.StateProcesses].toArray();
  for (const auto& json_vref : process_array)
  {
    JSONObject::Deserializer deserializer{json_vref.toObject()};
    auto proc = deserialize_interface(pl, deserializer, &s);
    if (proc)
      s.stateProcesses.add(proc);
    else
      SCORE_TODO;
  }
}

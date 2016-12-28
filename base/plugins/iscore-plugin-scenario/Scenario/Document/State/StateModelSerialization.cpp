#include <Scenario/Document/State/StateModel.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <Process/StateProcessFactoryList.hpp>

#include <Process/State/MessageNode.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <iscore/model/ModelMetadata.hpp>

#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/model/tree/TreeNode.hpp>

namespace Scenario
{
class ConstraintModel;
class EventModel;
}
template <typename T>
class Reader;
template <typename T>
class Writer;
template <typename model>
class IdentifiedObject;


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamReader::read(const Scenario::StateModel& s)
{
  m_stream << s.m_eventId << s.m_previousConstraint << s.m_nextConstraint
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
ISCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamWriter::write(Scenario::StateModel& s)
{
  m_stream >> s.m_eventId >> s.m_previousConstraint >> s.m_nextConstraint
      >> s.m_heightPercentage;

  // Message tree
  Process::MessageNode n;
  m_stream >> n;
  s.m_messageItemModel = new Scenario::MessageItemModel{s.m_stack, s, &s};
  s.messages() = n;

  // Processes plugins
  int32_t process_count;
  m_stream >> process_count;
  auto& pl = components.interfaces<Process::StateProcessList>();
  for (; process_count-- > 0;)
  {
    auto proc = deserialize_interface(pl, *this, &s);
    if (proc)
      s.stateProcesses.add(proc);
    else
      ISCORE_TODO;
  }

  checkDelimiter();
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
JSONObjectReader::read(const Scenario::StateModel& s)
{
  obj[strings.Event] = toJsonValue(s.m_eventId);
  obj[strings.PreviousConstraint] = toJsonValue(s.m_previousConstraint);
  obj[strings.NextConstraint] = toJsonValue(s.m_nextConstraint);
  obj[strings.HeightPercentage] = s.m_heightPercentage;

  // Message tree
  obj[strings.Messages] = toJsonObject(s.m_messageItemModel->rootNode());

  // Processes plugins
  obj[strings.StateProcesses] = toJsonArray(s.stateProcesses);
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
JSONObjectWriter::write(Scenario::StateModel& s)
{
  s.m_eventId = fromJsonValue<Id<Scenario::EventModel>>(obj[strings.Event]);
  s.m_previousConstraint
      = fromJsonValue<OptionalId<Scenario::ConstraintModel>>(
          obj[strings.PreviousConstraint]);
  s.m_nextConstraint = fromJsonValue<OptionalId<Scenario::ConstraintModel>>(
      obj[strings.NextConstraint]);
  s.m_heightPercentage = obj[strings.HeightPercentage].toDouble();

  // Message tree
  s.m_messageItemModel = new Scenario::MessageItemModel{s.m_stack, s, &s};
  s.messages() = fromJsonObject<Process::MessageNode>(obj[strings.Messages]);

  // Processes plugins
  auto& pl = components.interfaces<Process::StateProcessList>();

  QJsonArray process_array = obj[strings.StateProcesses].toArray();
  for (const auto& json_vref : process_array)
  {
    JSONObject::Deserializer deserializer{json_vref.toObject()};
    auto proc = deserialize_interface(pl, deserializer, &s);
    if (proc)
      s.stateProcesses.add(proc);
    else
      ISCORE_TODO;
  }
}

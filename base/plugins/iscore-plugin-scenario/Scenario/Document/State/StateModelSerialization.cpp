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
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/tools/TreeNode.hpp>

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
Visitor<Reader<DataStream>>::readFrom(const Scenario::StateModel& s)
{
  readFrom(static_cast<const iscore::Entity<Scenario::StateModel>&>(s));

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
Visitor<Writer<DataStream>>::writeTo(Scenario::StateModel& s)
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
Visitor<Reader<JSONObject>>::readFrom(const Scenario::StateModel& s)
{
  readFrom(static_cast<const iscore::Entity<Scenario::StateModel>&>(s));

  m_obj["Event"] = toJsonValue(s.m_eventId);
  m_obj["PreviousConstraint"] = toJsonValue(s.m_previousConstraint);
  m_obj["NextConstraint"] = toJsonValue(s.m_nextConstraint);
  m_obj["HeightPercentage"] = s.m_heightPercentage;

  // Message tree
  m_obj["Messages"] = toJsonObject(s.m_messageItemModel->rootNode());

  // Processes plugins
  m_obj["StateProcesses"] = toJsonArray(s.stateProcesses);
}

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(Scenario::StateModel& s)
{
  s.m_eventId = fromJsonValue<Id<Scenario::EventModel>>(m_obj["Event"]);
  s.m_previousConstraint
      = fromJsonValue<OptionalId<Scenario::ConstraintModel>>(
          m_obj["PreviousConstraint"]);
  s.m_nextConstraint = fromJsonValue<OptionalId<Scenario::ConstraintModel>>(
      m_obj["NextConstraint"]);
  s.m_heightPercentage = m_obj["HeightPercentage"].toDouble();

  // Message tree
  s.m_messageItemModel = new Scenario::MessageItemModel{s.m_stack, s, &s};
  s.messages() = fromJsonObject<Process::MessageNode>(m_obj["Messages"]);

  // Processes plugins
  auto& pl = components.interfaces<Process::StateProcessList>();

  QJsonArray process_array = m_obj["StateProcesses"].toArray();
  for (const auto& json_vref : process_array)
  {
    Deserializer<JSONObject> deserializer{json_vref.toObject()};
    auto proc = deserialize_interface(pl, deserializer, &s);
    if (proc)
      s.stateProcesses.add(proc);
    else
      ISCORE_TODO;
  }
}

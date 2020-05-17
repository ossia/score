// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/ControlMessage.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/State/MessageNode.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntityMapSerialization.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(const Scenario::StateModel& s)
{
  m_stream << s.m_eventId << s.m_previousInterval << s.m_nextInterval << s.m_heightPercentage;

  // Message tree
  m_stream << s.m_messageItemModel->rootNode();
  m_stream << s.m_controlItemModel->messages();

  // Processes plugins
  m_stream << s.stateProcesses;

  insertDelimiter();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(Scenario::StateModel& s)
{
  m_stream >> s.m_eventId >> s.m_previousInterval >> s.m_nextInterval >> s.m_heightPercentage;

  // Message tree
  Process::MessageNode n;
  m_stream >> n;
  s.m_messageItemModel = new Scenario::MessageItemModel{s, &s};
  s.messages() = n;

  // TODO load control tree
  std::vector<Process::ControlMessage> ctrls;
  m_stream >> ctrls;
  s.m_controlItemModel = new Scenario::ControlItemModel{s, &s};
  s.m_controlItemModel->replaceWith(std::move(ctrls));

  // Processes plugins
  EntityMapSerializer::writeTo<Process::ProcessFactoryList>(
      *this, s.stateProcesses, s.m_context, &s);

  checkDelimiter();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void JSONReader::read(const Scenario::StateModel& s)
{
  obj[strings.Event] = s.m_eventId;
  obj[strings.PreviousInterval] = s.m_previousInterval;
  obj[strings.NextInterval] = s.m_nextInterval;
  obj[strings.HeightPercentage] = s.m_heightPercentage;

  // Message tree
  obj[strings.Messages] = s.m_messageItemModel->rootNode();
  obj["Controls"] = s.m_controlItemModel->messages();

  // Processes plugins
  obj[strings.StateProcesses] = s.stateProcesses;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void JSONWriter::write(Scenario::StateModel& s)
{
  s.m_eventId <<= obj[strings.Event];
  s.m_previousInterval <<= obj[strings.PreviousInterval];
  s.m_nextInterval <<= obj[strings.NextInterval];
  s.m_heightPercentage = obj[strings.HeightPercentage].toDouble();

  // Message tree
  s.m_messageItemModel = new Scenario::MessageItemModel{s, &s};
  s.messages() = obj[strings.Messages].to<Process::MessageNode>();

  auto ctrls = obj["Controls"].to<std::vector<Process::ControlMessage>>();
  s.m_controlItemModel = new Scenario::ControlItemModel{s, &s};
  s.m_controlItemModel->replaceWith(std::move(ctrls));

  // Processes plugins
  EntityMapSerializer::writeTo<Process::ProcessFactoryList>(
      JSONWriter(obj[strings.StateProcesses].obj), s.stateProcesses, s.m_context, &s);
}

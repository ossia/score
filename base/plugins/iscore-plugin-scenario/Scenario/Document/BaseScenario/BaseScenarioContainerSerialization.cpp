// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QJsonObject>
#include <QJsonValue>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <algorithm>

#include "BaseScenarioContainer.hpp"
#include <Scenario/Document/State/StateModel.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <typename T>
class Reader;
template <typename T>
class Writer;


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(
    const Scenario::BaseScenarioContainer& base_scenario)
{
  readFrom(*base_scenario.m_constraint);

  readFrom(*base_scenario.m_startNode);
  readFrom(*base_scenario.m_endNode);

  readFrom(*base_scenario.m_startEvent);
  readFrom(*base_scenario.m_endEvent);

  readFrom(*base_scenario.m_startState);
  readFrom(*base_scenario.m_endState);
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamWriter::write(
    Scenario::BaseScenarioContainer& base_scenario)
{
  using namespace Scenario;
  base_scenario.m_constraint
      = new ConstraintModel{*this, base_scenario.m_parent};

  base_scenario.m_startNode = new TimeSyncModel{*this, base_scenario.m_parent};
  base_scenario.m_endNode = new TimeSyncModel{*this, base_scenario.m_parent};

  base_scenario.m_startEvent = new EventModel{*this, base_scenario.m_parent};
  base_scenario.m_endEvent = new EventModel{*this, base_scenario.m_parent};

  auto& stack
      = iscore::IDocument::documentContext(base_scenario.parentObject())
            .commandStack;
  base_scenario.m_startState
      = new StateModel{*this, stack, base_scenario.m_parent};
  base_scenario.m_endState
      = new StateModel{*this, stack, base_scenario.m_parent};

  Scenario::SetPreviousConstraint(
      *base_scenario.m_endState, *base_scenario.m_constraint);
  Scenario::SetNextConstraint(
      *base_scenario.m_startState, *base_scenario.m_constraint);
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void JSONObjectReader::read(
    const Scenario::BaseScenarioContainer& base_scenario)
{
  obj["Constraint"] = toJsonObject(*base_scenario.m_constraint);

  obj["StartTimeSync"] = toJsonObject(*base_scenario.m_startNode);
  obj["EndTimeSync"] = toJsonObject(*base_scenario.m_endNode);

  obj["StartEvent"] = toJsonObject(*base_scenario.m_startEvent);
  obj["EndEvent"] = toJsonObject(*base_scenario.m_endEvent);

  obj["StartState"] = toJsonObject(*base_scenario.m_startState);
  obj["EndState"] = toJsonObject(*base_scenario.m_endState);
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void JSONObjectWriter::write(
    Scenario::BaseScenarioContainer& base_scenario)
{
  using namespace Scenario;
  base_scenario.m_constraint = new ConstraintModel{
      JSONObject::Deserializer{obj["Constraint"].toObject()},
      base_scenario.m_parent};

  base_scenario.m_startNode = new TimeSyncModel{
      JSONObject::Deserializer{obj["StartTimeSync"].toObject()},
      base_scenario.m_parent};
  base_scenario.m_endNode = new TimeSyncModel{
      JSONObject::Deserializer{obj["EndTimeSync"].toObject()},
      base_scenario.m_parent};

  base_scenario.m_startEvent = new EventModel{
      JSONObject::Deserializer{obj["StartEvent"].toObject()},
      base_scenario.m_parent};
  base_scenario.m_endEvent
      = new EventModel{JSONObject::Deserializer{obj["EndEvent"].toObject()},
                       base_scenario.m_parent};

  auto& stack
      = iscore::IDocument::documentContext(base_scenario.parentObject())
            .commandStack;
  base_scenario.m_startState = new StateModel{
      JSONObject::Deserializer{obj["StartState"].toObject()}, stack,
      base_scenario.m_parent};
  base_scenario.m_endState
      = new StateModel{JSONObject::Deserializer{obj["EndState"].toObject()},
                       stack, base_scenario.m_parent};

  Scenario::SetPreviousConstraint(
      *base_scenario.m_endState, *base_scenario.m_constraint);
  Scenario::SetNextConstraint(
      *base_scenario.m_startState, *base_scenario.m_constraint);
}

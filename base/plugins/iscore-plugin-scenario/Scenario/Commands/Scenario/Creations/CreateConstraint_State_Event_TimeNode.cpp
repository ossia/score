// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/tools/RandomNameProvider.hpp>

#include <QByteArray>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <vector>

#include "CreateConstraint_State_Event_TimeNode.hpp"
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State_Event.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>

namespace Scenario
{
namespace Command
{
CreateConstraint_State_Event_TimeNode::CreateConstraint_State_Event_TimeNode(
    const Scenario::ProcessModel& scenario,
    Id<StateModel>
        startState,
    TimeVal date,
    double endStateY)
    : m_newTimeNode{getStrongId(scenario.timeNodes)}
    , m_createdName{RandomNameProvider::generateRandomName()}
    , m_command{scenario, std::move(startState), m_newTimeNode, endStateY}
    , m_date{std::move(date)}
{
}

void CreateConstraint_State_Event_TimeNode::undo(const iscore::DocumentContext& ctx) const
{
  m_command.undo(ctx);

  ScenarioCreate<TimeNodeModel>::undo(
      m_newTimeNode, m_command.scenarioPath().find(ctx));
}

void CreateConstraint_State_Event_TimeNode::redo(const iscore::DocumentContext& ctx) const
{
  auto& scenar = m_command.scenarioPath().find(ctx);

  // Create the end timenode
  ScenarioCreate<TimeNodeModel>::redo(
      m_newTimeNode,
      {m_command.endStateY(), m_command.endStateY()},
      m_date,
      scenar);

  scenar.timeNode(m_newTimeNode).metadata().setName(m_createdName);

  // The event + state + constraint between
  m_command.redo(ctx);
}

void CreateConstraint_State_Event_TimeNode::serializeImpl(
    DataStreamInput& s) const
{
  s << m_newTimeNode << m_createdName << m_command.serialize() << m_date;
}

void CreateConstraint_State_Event_TimeNode::deserializeImpl(
    DataStreamOutput& s)
{
  QByteArray b;
  s >> m_newTimeNode >> m_createdName >> b >> m_date;

  m_command.deserialize(b);
}
}
}

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

#include "CreateConstraint_State_Event_TimeSync.hpp"
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State_Event.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>

namespace Scenario
{
namespace Command
{
CreateConstraint_State_Event_TimeSync::CreateConstraint_State_Event_TimeSync(
    const Scenario::ProcessModel& scenario,
    Id<StateModel>
        startState,
    TimeVal date,
    double endStateY)
    : m_newTimeSync{getStrongId(scenario.timeSyncs)}
    , m_createdName{RandomNameProvider::generateRandomName()}
    , m_command{scenario, std::move(startState), m_newTimeSync, endStateY}
    , m_date{std::move(date)}
{
}

void CreateConstraint_State_Event_TimeSync::undo(const iscore::DocumentContext& ctx) const
{
  m_command.undo(ctx);

  ScenarioCreate<TimeSyncModel>::undo(
      m_newTimeSync, m_command.scenarioPath().find(ctx));
}

void CreateConstraint_State_Event_TimeSync::redo(const iscore::DocumentContext& ctx) const
{
  auto& scenar = m_command.scenarioPath().find(ctx);

  // Create the end timesync
  ScenarioCreate<TimeSyncModel>::redo(
      m_newTimeSync,
      {m_command.endStateY(), m_command.endStateY()},
      m_date,
      scenar);

  scenar.timeSync(m_newTimeSync).metadata().setName(m_createdName);

  // The event + state + constraint between
  m_command.redo(ctx);
}

void CreateConstraint_State_Event_TimeSync::serializeImpl(
    DataStreamInput& s) const
{
  s << m_newTimeSync << m_createdName << m_command.serialize() << m_date;
}

void CreateConstraint_State_Event_TimeSync::deserializeImpl(
    DataStreamOutput& s)
{
  QByteArray b;
  s >> m_newTimeSync >> m_createdName >> b >> m_date;

  m_command.deserialize(b);
}
}
}

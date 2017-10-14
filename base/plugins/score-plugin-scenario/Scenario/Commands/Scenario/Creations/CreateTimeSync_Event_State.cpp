// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <score/tools/RandomNameProvider.hpp>

#include <QByteArray>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <vector>

#include "CreateTimeSync_Event_State.hpp"
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateEvent_State.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/EntityMap.hpp>

namespace Scenario
{
namespace Command
{
CreateTimeSync_Event_State::CreateTimeSync_Event_State(
    const Scenario::ProcessModel& scenario,
    TimeVal date,
    double stateY)
    : m_newTimeSync{getStrongId(scenario.timeSyncs)}
    , m_createdName{RandomNameProvider::generateName<TimeSyncModel>()}
    , m_date{std::move(date)}
    , m_command{scenario, m_newTimeSync, stateY}
{
}

void CreateTimeSync_Event_State::undo(const score::DocumentContext& ctx) const
{
  m_command.undo(ctx);

  ScenarioCreate<TimeSyncModel>::undo(
      m_newTimeSync, m_command.scenarioPath().find(ctx));
}

void CreateTimeSync_Event_State::redo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_command.scenarioPath().find(ctx);

  // Create the node
  ScenarioCreate<TimeSyncModel>::redo(
      m_newTimeSync, {0.4, 0.6}, m_date, scenar);

  // And the event
  m_command.redo(ctx);
}

void CreateTimeSync_Event_State::serializeImpl(DataStreamInput& s) const
{
  s << m_newTimeSync << m_date << m_command.serialize();
}

void CreateTimeSync_Event_State::deserializeImpl(DataStreamOutput& s)
{
  QByteArray b;
  s >> m_newTimeSync >> m_date >> b;

  m_command.deserialize(b);
}
}
}

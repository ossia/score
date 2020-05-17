// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioTriggerCommandFactory.hpp"

#include <Scenario/Commands/TimeSync/AddTrigger.hpp>
#include <Scenario/Commands/TimeSync/RemoveTrigger.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Scenario
{
namespace Command
{

bool ScenarioTriggerCommandFactory::matches(const TimeSyncModel& tn) const
{
  return dynamic_cast<Scenario::ProcessModel*>(tn.parent());
}

score::Command*
ScenarioTriggerCommandFactory::make_addTriggerCommand(const TimeSyncModel& tn) const
{
  if (dynamic_cast<Scenario::ProcessModel*>(tn.parent()))
  {
    return new Scenario::Command::AddTrigger<Scenario::ProcessModel>(tn);
  }
  return nullptr;
}

score::Command*
ScenarioTriggerCommandFactory::make_removeTriggerCommand(const TimeSyncModel& tn) const
{
  if (dynamic_cast<Scenario::ProcessModel*>(tn.parent()))
  {
    return new Scenario::Command::RemoveTrigger<Scenario::ProcessModel>(tn);
  }
  return nullptr;
}
}
}

// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QByteArray>
#include <Scenario/Commands/TimeSync/AddTrigger.hpp>
#include <Scenario/Commands/TimeSync/RemoveTrigger.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include "BaseScenarioTriggerCommandFactory.hpp"
#include <Scenario/Process/ScenarioInterface.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
bool BaseScenarioTriggerCommandFactory::matches(const TimeSyncModel& tn) const
{
  return dynamic_cast<BaseScenario*>(tn.parent());
}

iscore::Command*
BaseScenarioTriggerCommandFactory::make_addTriggerCommand(
    const TimeSyncModel& tn) const
{
  if (dynamic_cast<BaseScenario*>(tn.parent()))
  {
    return new Scenario::Command::AddTrigger<BaseScenario>(tn);
  }
  return nullptr;
}

iscore::Command*
BaseScenarioTriggerCommandFactory::make_removeTriggerCommand(
    const TimeSyncModel& tn) const
{
  if (dynamic_cast<BaseScenario*>(tn.parent()))
  {
    return new Scenario::Command::RemoveTrigger<BaseScenario>(tn);
  }
  return nullptr;
}
}
}

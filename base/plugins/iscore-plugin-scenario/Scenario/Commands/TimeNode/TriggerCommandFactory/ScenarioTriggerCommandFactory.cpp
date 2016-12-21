#include <QByteArray>
#include <Scenario/Commands/TimeNode/AddTrigger.hpp>
#include <Scenario/Commands/TimeNode/RemoveTrigger.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include "ScenarioTriggerCommandFactory.hpp"
#include <Scenario/Process/ScenarioInterface.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
namespace Scenario
{
namespace Command
{

bool ScenarioTriggerCommandFactory::matches(const TimeNodeModel& tn) const
{
  return dynamic_cast<Scenario::ProcessModel*>(tn.parent());
}

iscore::Command*
ScenarioTriggerCommandFactory::make_addTriggerCommand(
    const TimeNodeModel& tn) const
{
  if (dynamic_cast<Scenario::ProcessModel*>(tn.parent()))
  {
    return new Scenario::Command::AddTrigger<Scenario::ProcessModel>(tn);
  }
  return nullptr;
}

iscore::Command*
ScenarioTriggerCommandFactory::make_removeTriggerCommand(
    const TimeNodeModel& tn) const
{
  if (dynamic_cast<Scenario::ProcessModel*>(tn.parent()))
  {
    return new Scenario::Command::RemoveTrigger<Scenario::ProcessModel>(tn);
  }
  return nullptr;
}
}
}

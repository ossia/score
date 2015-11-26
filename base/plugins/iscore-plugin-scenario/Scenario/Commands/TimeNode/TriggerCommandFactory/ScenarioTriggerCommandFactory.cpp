#include "ScenarioTriggerCommandFactory.hpp"
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Commands/TimeNode/AddTrigger.hpp>
#include <Scenario/Commands/TimeNode/RemoveTrigger.hpp>

bool ScenarioTriggerCommandFactory::matches(const TimeNodeModel& tn) const
{
  return dynamic_cast<Scenario::ScenarioModel*>(tn.parentScenario());
}

iscore::SerializableCommand*ScenarioTriggerCommandFactory::make_addTriggerCommand(const TimeNodeModel& tn) const
{
  if(dynamic_cast<Scenario::ScenarioModel*>(tn.parentScenario()))
  {
    return new Scenario::Command::AddTrigger<Scenario::ScenarioModel>(tn);
  }
  return nullptr;
}

iscore::SerializableCommand*ScenarioTriggerCommandFactory::make_removeTriggerCommand(const TimeNodeModel& tn) const
{
  if(dynamic_cast<Scenario::ScenarioModel*>(tn.parentScenario()))
  {
    return new Scenario::Command::RemoveTrigger<Scenario::ScenarioModel>(tn);
  }
  return nullptr;
}

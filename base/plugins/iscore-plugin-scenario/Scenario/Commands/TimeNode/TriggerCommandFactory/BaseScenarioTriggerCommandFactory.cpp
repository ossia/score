#include "BaseScenarioTriggerCommandFactory.hpp"
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Commands/TimeNode/AddTrigger.hpp>
#include <Scenario/Commands/TimeNode/RemoveTrigger.hpp>

bool BaseScenarioTriggerCommandFactory::matches(
        const TimeNodeModel& tn) const
{
  return dynamic_cast<BaseScenario*>(tn.parentScenario());
}

iscore::SerializableCommand* BaseScenarioTriggerCommandFactory::make_addTriggerCommand(
        const TimeNodeModel& tn) const
{
  if(dynamic_cast<BaseScenario*>(tn.parentScenario()))
  {
    return new Scenario::Command::AddTrigger<BaseScenario>(tn);
  }
  return nullptr;
}

iscore::SerializableCommand* BaseScenarioTriggerCommandFactory::make_removeTriggerCommand(
        const TimeNodeModel& tn) const
{
  if(dynamic_cast<BaseScenario*>(tn.parentScenario()))
  {
    return new Scenario::Command::RemoveTrigger<BaseScenario>(tn);
  }
  return nullptr;
}

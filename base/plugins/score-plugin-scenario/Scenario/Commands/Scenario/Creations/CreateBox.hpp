#pragma once
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <score/command/AggregateCommand.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
namespace Scenario
{
namespace Command
{

class SCORE_PLUGIN_SCENARIO_EXPORT CreateBox final
    : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      CreateBox,
      "Create a box")

  public:
};

SCORE_PLUGIN_SCENARIO_EXPORT
void MakeBox(
      const score::CommandStackFacade& stack,
      const Scenario::ProcessModel& scenario,
      TimeVal date,
      TimeVal endDate,
      double Y);

}
}

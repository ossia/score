#pragma once
#include <Process/Process.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <score/command/PropertyCommand.hpp>
namespace Scenario
{
namespace Command
{

class SetProcessPriority final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), SetProcessPriority, "Set process priority")
public:
  SetProcessPriority(const Process::ProcessModel& path, int32_t newval)
      : score::PropertyCommand{path, "processPriority",
                                QVariant::fromValue(newval)}
  {
  }
};

class SetProcessPriorityOverride final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      SetProcessPriorityOverride,
      "Set process priority override")
public:
  SetProcessPriorityOverride(const Process::ProcessModel& path, bool newval)
      : score::PropertyCommand{path, "processPriorityOverride",
                                QVariant::fromValue(newval)}
  {
  }
};
}
}

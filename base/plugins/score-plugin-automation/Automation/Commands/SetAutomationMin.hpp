#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <score/command/PropertyCommand.hpp>

namespace Automation
{
class ProcessModel;
class SetMin final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetMin, "Set automation minimum")
public:
  SetMin(const ProcessModel& path, double newval)
      : score::PropertyCommand{path, "min", newval}
  {
  }
};
}

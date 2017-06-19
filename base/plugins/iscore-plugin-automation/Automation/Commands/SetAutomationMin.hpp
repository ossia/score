#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <iscore/command/PropertyCommand.hpp>

namespace Automation
{
class ProcessModel;
class SetMin final : public iscore::PropertyCommand
{
  ISCORE_COMMAND_DECL(CommandFactoryName(), SetMin, "Set automation minimum")
public:
  SetMin(const ProcessModel& path, double newval)
      : iscore::PropertyCommand{path, "min", newval}
  {
  }
};
}

#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <score/command/PropertyCommand.hpp>

#include <State/Unit.hpp>

namespace Process
{
class ProcessModel;
}
namespace Automation
{
class SetMin final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetMin, "Set minimum")
public:
  SetMin(const Process::ProcessModel& path, double newval)
      : score::PropertyCommand{path, "min", newval}
  {
  }
};
class SetMax final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetMax, "Set maximum")
public:
  SetMax(const Process::ProcessModel& path, double newval)
      : score::PropertyCommand{path, "max", newval}
  {
  }
};

// MOVEME
class SetTween final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetTween, "Set tween")
public:
  SetTween(const Process::ProcessModel& path, bool newval)
      : score::PropertyCommand{path, "tween", newval}
  {
  }
};

class SetUnit final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetUnit, "Set unit")
public:
  SetUnit(const Process::ProcessModel& path, const State::Unit& newval)
      : score::PropertyCommand{path, "unit", QVariant::fromValue(newval)}
  {
  }
};
}

#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <score/command/PropertyCommand.hpp>

#include <State/Unit.hpp>

namespace Automation
{
class ProcessModel;
class SetMax final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetMax, "Set automation maximum")
public:
  SetMax(const ProcessModel& path, double newval)
      : score::PropertyCommand{path, "max", newval}
  {
  }
};

// MOVEME
class SetTween final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetTween, "Set automation tween")
public:
  SetTween(const ProcessModel& path, bool newval)
      : score::PropertyCommand{path, "tween", newval}
  {
  }
};

class SetUnit final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetUnit, "Set automation unit")
public:
  SetUnit(const ProcessModel& path, const State::Unit& newval)
      : score::PropertyCommand{path, "unit",
                                QVariant::fromValue(newval)}
  {
  }
};
}

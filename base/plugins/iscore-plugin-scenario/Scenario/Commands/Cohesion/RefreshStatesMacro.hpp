#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/AggregateCommand.hpp>

namespace Scenario
{
namespace Command
{
class RefreshStatesMacro final : public iscore::AggregateCommand
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), RefreshStatesMacro, "Refresh states")
};
}
}

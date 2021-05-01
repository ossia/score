#pragma once

#include <score/command/AggregateCommand.hpp>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

namespace Scenario
{
namespace Command
{
class ClearSelection final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ClearSelection, "Clear selection")
};
}
}

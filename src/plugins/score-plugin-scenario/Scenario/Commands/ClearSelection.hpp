#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>

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

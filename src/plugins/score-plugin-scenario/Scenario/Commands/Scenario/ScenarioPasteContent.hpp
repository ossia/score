#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>

namespace Scenario
{
namespace Command
{

class ScenarioPasteContent final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ScenarioPasteContent, "Paste content in scenario")
};
}
}

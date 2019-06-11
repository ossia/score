#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>

namespace Dataflow
{
class CreateModulation final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(),
      CreateModulation,
      "Create modulation")
};
}

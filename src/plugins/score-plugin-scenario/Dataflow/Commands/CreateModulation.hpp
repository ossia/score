#pragma once
#include <score/command/AggregateCommand.hpp>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

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

#pragma once
#include <Process/ProcessFactory.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>

namespace Scenario
{
namespace Command
{
class AddProcessInNewSlot final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), AddProcessInNewSlot,
      "Create a process in a new slot")
};

class DuplicateProcess final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), DuplicateProcess, "Duplicate a process")
};
}
}

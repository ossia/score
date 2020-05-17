#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>

namespace Scenario
{
namespace Command
{
/**
 * @brief The CreateStateMacro class
 *
 * Used to quickly create a state from data coming from outside.
 * For instance creating a StateModel and adding data inside.
 *
 */
class CreateStateMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), CreateStateMacro, "Drop a state")
public:
};
}
}

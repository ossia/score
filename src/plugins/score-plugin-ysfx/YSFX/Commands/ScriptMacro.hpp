#pragma once
#include <YSFX/Commands/CommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>

namespace YSFX
{
class ScriptMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(YSFX::CommandFactoryName(), ScriptMacro, "Script operation")
};
}

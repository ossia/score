#pragma once
#include <JS/Commands/JSCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>

namespace JS
{
class ScriptMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(JS::CommandFactoryName(), ScriptMacro, "Script operation")
};
}

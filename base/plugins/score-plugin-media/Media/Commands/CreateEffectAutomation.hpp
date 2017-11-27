#pragma once
#include <score/command/AggregateCommand.hpp>
#include <Media/Commands/MediaCommandFactory.hpp>

namespace Media
{
namespace Commands
{
class CreateEffectAutomation final : public score::AggregateCommand
{
        SCORE_COMMAND_DECL(Media::CommandFactoryName(), CreateEffectAutomation, "Create effect automation")
};
}
}

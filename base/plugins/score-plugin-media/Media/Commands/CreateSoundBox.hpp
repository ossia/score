#pragma once
#include <score/command/AggregateCommand.hpp>
namespace Media
{
namespace Commands
{

class CreateSoundBoxMacro final : public score::AggregateCommand
{
        SCORE_COMMAND_DECL(Media::CommandFactoryName(), CreateSoundBoxMacro, "Add sounds in a box")
};

class CreateSoundBoxesMacro final : public score::AggregateCommand
{
        SCORE_COMMAND_DECL(Media::CommandFactoryName(), CreateSoundBoxesMacro, "Add sounds in sequence")
};

}
}

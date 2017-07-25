#pragma once
#include <iscore/command/AggregateCommand.hpp>
namespace Media
{
namespace Commands
{

class CreateSoundBoxMacro final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(Media::CommandFactoryName(), CreateSoundBoxMacro, "Add sounds in a box")
};

class CreateSoundBoxesMacro final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(Media::CommandFactoryName(), CreateSoundBoxesMacro, "Add sounds in sequence")
};

}
}

#pragma once
#include <iscore/command/AggregateCommand.hpp>
#include <Commands/IScoreCohesionCommandFactory.hpp>

// TODO rename file
class SnapshotStatesMacro final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(IScoreCohesionCommandFactoryName(), SnapshotStatesMacro, "SnapshotStatesMacro")

};

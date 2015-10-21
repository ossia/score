#pragma once
#include <iscore/command/AggregateCommand.hpp>
#include <Commands/IScoreCohesionCommandFactory.hpp>

// MOVEME
class SnapshotStatesMacro : public iscore::AggregateCommand
{
        ISCORE_AGGREGATE_COMMAND_DECL(IScoreCohesionCommandFactoryName(),
                                      SnapshotStatesMacro,
                                      "SnapshotStatesMacro")

};

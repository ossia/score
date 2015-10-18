#pragma once
#include <iscore/command/AggregateCommand.hpp>

class InterpolateMacro : public iscore::AggregateCommand
{
        ISCORE_AGGREGATE_COMMAND_DECL("IScoreCohesionControl", InterpolateMacro, "InterpolateMacro")
};

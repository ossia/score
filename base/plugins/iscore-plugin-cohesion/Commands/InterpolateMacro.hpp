#pragma once
#include <iscore/command/AggregateCommand.hpp>

class InterpolateMacro : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL("IScoreCohesionControl", "InterpolateMacro", "InterpolateMacro")
    public:
        InterpolateMacro():
            AggregateCommand{factoryName(),
                             commandName(),
                             description()}
        { }
};

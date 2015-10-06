#pragma once
#include <iscore/command/AggregateCommand.hpp>

class CreateStatesFromParametersInEvents : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL("IScoreCohesionControl", "CreateStatesFromParametersInEvents", "CreateStatesFromParametersInEvents")
    public:
        CreateStatesFromParametersInEvents():
            AggregateCommand{factoryName(),
                             commandName(),
                             description()}
        { }
};

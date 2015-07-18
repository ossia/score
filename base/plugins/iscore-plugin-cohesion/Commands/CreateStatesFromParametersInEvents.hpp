#pragma once
#include <iscore/command/AggregateCommand.hpp>

class CreateStatesFromParametersInEvents : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL("CreateStatesFromParametersInEvents", "CreateStatesFromParametersInEvents")
    public:
        CreateStatesFromParametersInEvents():
            AggregateCommand{"IScoreCohesionControl",
                             commandName(),
                             description()}
        { }
};

#pragma once
#include <iscore/command/AggregateCommand.hpp>

class CreateStatesFromParametersInEvents : public iscore::AggregateCommand
{
    public:
        CreateStatesFromParametersInEvents():
            AggregateCommand{"IScoreCohesionControl",
                             "CreateStatesFromParametersInEvents",
                             "TODO"}
        { }
};

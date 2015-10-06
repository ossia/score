#pragma once
#include <iscore/command/AggregateCommand.hpp>

class CreateCurvesFromAddressesInConstraints : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL("IScoreCohesionControl", "CreateCurvesFromAddressesInConstraints", "CreateCurvesFromAddressesInConstraints")
    public:
        CreateCurvesFromAddressesInConstraints():
            AggregateCommand{factoryName(),
                             commandName(),
                             description()}
        { }
};

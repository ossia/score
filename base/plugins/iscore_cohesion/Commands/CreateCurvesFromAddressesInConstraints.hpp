#pragma once
#include <public_interface/command/AggregateCommand.hpp>

class CreateCurvesFromAddressesInConstraints : public iscore::AggregateCommand
{
    public:
        CreateCurvesFromAddressesInConstraints():
            AggregateCommand{"IScoreCohesionControl",
                             "CreateCurvesFromAddressesInConstraints",
                             "TODO"}
        { }
};

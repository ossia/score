#pragma once
#include <core/presenter/command/AggregateCommand.hpp>

class CreateCurvesFromAddressesInConstraints : public iscore::AggregateCommand
{
    public:
        CreateCurvesFromAddressesInConstraints():
            AggregateCommand{"IScoreCohesionControl",
                             "CreateCurvesFromAddressesInConstraints",
                             "TODO"}
        { }
};

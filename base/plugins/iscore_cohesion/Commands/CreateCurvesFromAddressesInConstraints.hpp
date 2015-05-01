#pragma once
#include <iscore/command/AggregateCommand.hpp>

class CreateCurvesFromAddressesInConstraints : public iscore::AggregateCommand
{
    public:
        CreateCurvesFromAddressesInConstraints():
            AggregateCommand{"IScoreCohesionControl",
                             "CreateCurvesFromAddressesInConstraints",
                             "CreateCurvesFromAddressesInConstraints"}
        { }
};


class InterpolateMacro : public iscore::AggregateCommand
{
    public:
        InterpolateMacro():
            AggregateCommand{"IScoreCohesionControl",
                             "InterpolateMacro",
                             "InterpolateMacro"}
        { }
};

#pragma once
#include <iscore/command/AggregateCommand.hpp>

class CreateCurvesFromAddressesInConstraints : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL_OBSOLETE("CreateCurvesFromAddressesInConstraints", "CreateCurvesFromAddressesInConstraints")
    public:
        CreateCurvesFromAddressesInConstraints():
            AggregateCommand{"IScoreCohesionControl",
                             commandName(),
                             description()}
        { }
};


class InterpolateMacro : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL_OBSOLETE("InterpolateMacro", "InterpolateMacro")
    public:
        InterpolateMacro():
            AggregateCommand{"IScoreCohesionControl",
                             commandName(),
                             description()}
        { }
};

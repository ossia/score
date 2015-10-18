#pragma once
#include <iscore/command/AggregateCommand.hpp>

class CreateCurvesFromAddressesInConstraints : public iscore::AggregateCommand
{
        ISCORE_AGGREGATE_COMMAND_DECL("IScoreCohesionControl", CreateCurvesFromAddressesInConstraints, "CreateCurvesFromAddressesInConstraints")
};

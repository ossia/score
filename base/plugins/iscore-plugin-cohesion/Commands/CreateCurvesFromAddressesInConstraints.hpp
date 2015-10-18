#pragma once
#include <iscore/command/AggregateCommand.hpp>
#include <Commands/IScoreCohesionCommandFactory.hpp>

class CreateCurvesFromAddressesInConstraints : public iscore::AggregateCommand
{
        ISCORE_AGGREGATE_COMMAND_DECL(IScoreCohesionCommandFactoryName(),
                                      CreateCurvesFromAddressesInConstraints,
                                      "CreateCurvesFromAddressesInConstraints")
};

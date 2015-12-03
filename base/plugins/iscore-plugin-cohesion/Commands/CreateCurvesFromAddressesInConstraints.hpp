#pragma once
#include <iscore/command/AggregateCommand.hpp>
#include <Commands/IScoreCohesionCommandFactory.hpp>

class CreateCurvesFromAddressesInConstraints final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(IScoreCohesionCommandFactoryName(), CreateCurvesFromAddressesInConstraints, "CreateCurvesFromAddressesInConstraints")
};

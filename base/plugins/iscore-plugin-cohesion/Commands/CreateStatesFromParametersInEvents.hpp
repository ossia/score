#pragma once
#include <iscore/command/AggregateCommand.hpp>
#include <Commands/IScoreCohesionCommandFactory.hpp>

class CreateStatesFromParametersInEvents : public iscore::AggregateCommand
{
        ISCORE_AGGREGATE_COMMAND_DECL(IScoreCohesionCommandFactoryName(),
                                      CreateStatesFromParametersInEvents,
                                      "CreateStatesFromParametersInEvents")

};

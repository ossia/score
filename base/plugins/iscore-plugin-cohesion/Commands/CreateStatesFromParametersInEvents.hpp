#pragma once
#include <iscore/command/AggregateCommand.hpp>

class CreateStatesFromParametersInEvents : public iscore::AggregateCommand
{
        ISCORE_AGGREGATE_COMMAND_DECL("IScoreCohesionControl", CreateStatesFromParametersInEvents, "CreateStatesFromParametersInEvents")

};

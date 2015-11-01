#pragma once
#include <iscore/command/AggregateCommand.hpp>

class CreateProcessInExistingSlot : public iscore::AggregateCommand
{
        ISCORE_AGGREGATE_COMMAND_DECL(ScenarioCommandFactoryName(),
                                      CreateProcessInExistingSlot,
                                      "CreateProcessInExistingSlot")
};

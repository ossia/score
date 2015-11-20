#pragma once
#include <iscore/command/AggregateCommand.hpp>

class CreateProcessInNewSlot final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateProcessInNewSlot, "CreateProcessInNewSlot")
};

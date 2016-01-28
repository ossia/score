#pragma once
#include <iscore/command/AggregateCommand.hpp>

namespace Scenario
{
namespace Command
{
class CreateProcessInExistingSlot final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateProcessInExistingSlot, "Create a process in an existing slot")
};
}
}

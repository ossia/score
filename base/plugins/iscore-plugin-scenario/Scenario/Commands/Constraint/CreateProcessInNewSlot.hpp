#pragma once
#include <iscore/command/AggregateCommand.hpp>

namespace Scenario
{
namespace Command
{
class CreateProcessInNewSlot final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateProcessInNewSlot, "Create a process in a new slot")
};
}
}

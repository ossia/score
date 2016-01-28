#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <iscore/command/PropertyCommand.hpp>

namespace Automation
{
class ProcessModel;
class SetMin final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(CommandFactoryName(), SetMin, "Set curve minimum")
    public:

        SetMin(Path<ProcessModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "min", newval}
        {

        }
};
}

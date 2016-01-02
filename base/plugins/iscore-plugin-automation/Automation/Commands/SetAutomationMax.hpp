#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <iscore/command/PropertyCommand.hpp>

namespace Automation
{
class ProcessModel;
class SetMax final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(CommandFactoryName(), SetMax, "Set curve maximum")
        public:

        SetMax(Path<ProcessModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "max", newval}
        {

        }
};
}

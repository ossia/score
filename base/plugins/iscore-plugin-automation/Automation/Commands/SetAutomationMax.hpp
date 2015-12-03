#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <iscore/command/PropertyCommand.hpp>

class AutomationModel;
class SetAutomationMax final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(AutomationCommandFactoryName(), SetAutomationMax, "Set curve maximum")
        public:

        SetAutomationMax(Path<AutomationModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "max", newval}
        {

        }
};

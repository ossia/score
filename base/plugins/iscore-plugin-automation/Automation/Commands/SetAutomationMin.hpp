#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <iscore/command/PropertyCommand.hpp>

class AutomationModel;
class SetAutomationMin final : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL(AutomationCommandFactoryName(), SetAutomationMin, "Set curve minimum")
    public:

        SetAutomationMin(Path<AutomationModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "min", newval}
        {

        }
};

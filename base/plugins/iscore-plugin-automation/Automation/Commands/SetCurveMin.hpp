#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <iscore/command/PropertyCommand.hpp>

// TODO RENAMEME
class AutomationModel;
class SetAutomationMin : public iscore::PropertyCommand
{
        ISCORE_PROPERTY_COMMAND_DECL(AutomationCommandFactoryName(), SetAutomationMin, "Set curve minimum")
    public:

        SetAutomationMin(Path<AutomationModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "min", newval, factoryName(), commandName(), description()}
        {

        }
};

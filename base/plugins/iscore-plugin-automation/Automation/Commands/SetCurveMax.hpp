#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <iscore/command/PropertyCommand.hpp>

// TODO RENAMEME
class AutomationModel;
class SetAutomationMax : public iscore::PropertyCommand
{
        ISCORE_PROPERTY_COMMAND_DECL(AutomationCommandFactoryName(), SetAutomationMax, "Set curve maximum")
        public:

        SetAutomationMax(Path<AutomationModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "max", newval, factoryName(), commandName(), description()}
        {

        }
};

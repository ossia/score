#pragma once
#include <Commands/AutomationCommandFactory.hpp>
#include <iscore/command/PropertyCommand.hpp>

class AutomationModel;
class SetCurveMin : public iscore::PropertyCommand
{
        ISCORE_PROPERTY_COMMAND_DECL(AutomationCommandFactoryName(), SetCurveMin, "Set curve minimum")
    public:

        SetCurveMin(Path<AutomationModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "min", newval, factoryName(), commandName(), description()}
        {

        }
};

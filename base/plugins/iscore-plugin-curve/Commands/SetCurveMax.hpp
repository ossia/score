#pragma once
#include <Commands/AutomationCommandFactory.hpp>
#include <iscore/command/PropertyCommand.hpp>

class AutomationModel;
class SetCurveMax : public iscore::PropertyCommand
{
        ISCORE_PROPERTY_COMMAND_DECL(AutomationCommandFactoryName(), SetCurveMax, "Set curve maximum")
        public:

        SetCurveMax(Path<AutomationModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "max", newval, factoryName(), commandName(), description()}
        {

        }
};

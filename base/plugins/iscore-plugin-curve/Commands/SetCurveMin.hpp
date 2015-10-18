#pragma once
#include <iscore/command/PropertyCommand.hpp>

class SetCurveMin : public iscore::PropertyCommand
{
        ISCORE_PROPERTY_COMMAND_DECL("AutomationControl", SetCurveMin, "Set curve minimum")
    public:

        SetCurveMin(Path<AutomationModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "min", newval, factoryName(), commandName(), description()}
        {

        }
};

#pragma once
#include <iscore/command/PropertyCommand.hpp>

class SetCurveMin : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL_OBSOLETE("SetCurveMin", "Set curve minimum")
    public:
        ISCORE_PROPERTY_COMMAND_DEFAULT_CTOR(SetCurveMin, "AutomationControl")

        SetCurveMin(Path<AutomationModel>&& path, double newval):
            iscore::PropertyCommand{std::move(path), "min", newval,  "AutomationControl", commandName(), description()}
        {

        }
};

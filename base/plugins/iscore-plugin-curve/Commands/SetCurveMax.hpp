#pragma once
#include <iscore/command/PropertyCommand.hpp>

class SetCurveMax : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL("SetCurveMax", "Set curve minimum")
    public:
        ISCORE_PROPERTY_COMMAND_DEFAULT_CTOR(SetCurveMax, "AutomationControl")

        SetCurveMax(ObjectPath&& path, double newval):
            iscore::PropertyCommand{std::move(path), "max", newval, "AutomationControl", className(), description()}
        {

        }
};

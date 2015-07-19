#pragma once
#include <State/Address.hpp>
#include <iscore/command/PropertyCommand.hpp>

class ChangeAddress : public iscore::PropertyCommand
{
        ISCORE_COMMAND_DECL("ChangeAddress", "ChangeAddress")
    public:
        ISCORE_PROPERTY_COMMAND_DEFAULT_CTOR(ChangeAddress, "AutomationControl")

        ChangeAddress(
                ObjectPath&& path,
                const iscore::Address& newval):
            iscore::PropertyCommand{
                std::move(path),
                "address",
                QVariant::fromValue(newval),
                "AutomationControl", commandName(), description()}
        {

        }
};


#pragma once
#include <DeviceExplorer/Address/AddressSettings.hpp>

#include <iscore/tools/ObjectPath.hpp>
#include <iscore/command/SerializableCommand.hpp>

class ChangeAddress : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("ChangeAddress", "ChangeAddress")
    public:
        ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(ChangeAddress, "AutomationControl")
        ChangeAddress(
                ObjectPath&& path,
                const iscore::Address& newval);

    public:
        void undo();
        void redo();

    protected:
        void serializeImpl(QDataStream &) const;
        void deserializeImpl(QDataStream &);

    private:
        ObjectPath m_path;
        iscore::FullAddressSettings m_old, m_new;
};


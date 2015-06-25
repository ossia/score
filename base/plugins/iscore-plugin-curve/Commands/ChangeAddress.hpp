#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <State/Address.hpp>

// TODO maybe use property command.
class ChangeAddress : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("ChangeAddress", "ChangeAddress")
    public:
        ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(ChangeAddress, "AutomationControl")
        ChangeAddress(ObjectPath&& pointPath, const iscore::Address &addr);

        virtual void undo() override;
        virtual void redo() override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        ObjectPath m_path;

        iscore::Address m_newAddr;
        iscore::Address m_oldAddr;
};

#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <State/Message.hpp>

// TODO put to the new command format
class ChangeAddress : public iscore::SerializableCommand
{
    public:
        ChangeAddress();
        ChangeAddress(ObjectPath&& pointPath, const Address &addr);

        virtual void undo() override;
        virtual void redo() override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        ObjectPath m_path;

        Address m_newAddr;
        Address m_oldAddr;
};

#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

class ChangeAddress : public iscore::SerializableCommand
{
    public:
        ChangeAddress();
        ChangeAddress(ObjectPath&& pointPath, QString addr);

        virtual void undo() override;
        virtual void redo() override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        ObjectPath m_path;

        QString m_newAddr;
        QString m_oldAddr;
};

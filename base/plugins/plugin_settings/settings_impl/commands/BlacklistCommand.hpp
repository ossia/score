#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <QMap>
#include <QString>
class BlacklistCommand : public iscore::SerializableCommand
{
        // QUndoCommand interface
    public:
        BlacklistCommand(QString name, bool value);

        virtual void undo() override;
        virtual void redo() override;
        virtual bool mergeWith(const Command* other) override;

    protected:
        virtual void serializeImpl(QDataStream&) const override { }
        virtual void deserializeImpl(QDataStream&) override { }

        QMap<QString, bool> m_blacklistedState;
};

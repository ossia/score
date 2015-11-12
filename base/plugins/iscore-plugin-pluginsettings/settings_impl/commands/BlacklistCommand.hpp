#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <QMap>
#include <QString>
class BlacklistCommand : public iscore::SerializableCommand
{
        // QUndoCommand interface
    public:
        BlacklistCommand(QString name, bool value);

        void undo() const override;
        void redo() const override;
        //bool mergeWith(const Command* other) override;

    protected:
        void serializeImpl(QDataStream&) const override { }
        void deserializeImpl(QDataStream&) override { }

        QMap<QString, bool> m_blacklistedState;
};

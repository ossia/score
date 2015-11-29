#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <qmap.h>
#include <qstring.h>

class DataStreamInput;
class DataStreamOutput;

class BlacklistCommand : public iscore::SerializableCommand
{
        // QUndoCommand interface
    public:
        BlacklistCommand(QString name, bool value);

        void undo() const override;
        void redo() const override;
        //bool mergeWith(const Command* other) override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

        QMap<QString, bool> m_blacklistedState;

        // SerializableCommand interface
    public:
        const CommandParentFactoryKey&parentKey() const override;
        const CommandFactoryKey& key() const override;
        QString description() const override;
};

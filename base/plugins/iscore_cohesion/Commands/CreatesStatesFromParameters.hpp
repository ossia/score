#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <State/Message.hpp>

class CreateStatesFromParameters : public iscore::SerializableCommand
{
    public:
        CreateStatesFromParameters();
        CreateStatesFromParameters(ObjectPath&& event,
                                   QList<Message> messages);

        virtual void undo() override;
        virtual void redo() override;
        virtual bool mergeWith(const Command* other) override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        ObjectPath m_path;
        QList<Message> m_messages;

        QVector<QByteArray> m_serializedCommands;
};

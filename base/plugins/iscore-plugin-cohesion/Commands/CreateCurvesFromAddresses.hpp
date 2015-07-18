#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <State/Address.hpp>

class CreateCurvesFromAddresses : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("CreateCurvesFromAddresses", "CreateCurvesFromAddresses")
    public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(CreateCurvesFromAddresses, "IScoreCohesionControl")

        CreateCurvesFromAddresses(ObjectPath&& constraint,
                                  const QList<iscore::Address> &addresses);

        virtual void undo() override;
        virtual void redo() override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        ObjectPath m_path;
        QList<iscore::Address> m_addresses;

        QVector<QByteArray> m_serializedCommands;
};

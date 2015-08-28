#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <State/Address.hpp>

class ConstraintModel;
class CreateCurvesFromAddresses : public iscore::SerializableCommand
{
        //TODO now use this everywhere
        ISCORE_COMMAND_DECL("IScoreCohesionControl", "CreateCurvesFromAddresses", "CreateCurvesFromAddresses")
    public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(CreateCurvesFromAddresses)

        CreateCurvesFromAddresses(
          Path<ConstraintModel>&& constraint,
            const QList<iscore::Address> &addresses);

        virtual void undo() override;
        virtual void redo() override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        Path<ConstraintModel> m_path;
        QList<iscore::Address> m_addresses;

        QVector<QByteArray> m_serializedCommands;
};

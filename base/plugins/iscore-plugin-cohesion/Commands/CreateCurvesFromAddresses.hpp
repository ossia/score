#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <State/Address.hpp>

class ConstraintModel;
class CreateCurvesFromAddresses : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL("IScoreCohesionControl", CreateCurvesFromAddresses, "CreateCurvesFromAddresses")
    public:

        CreateCurvesFromAddresses(
          Path<ConstraintModel>&& constraint,
            const QList<iscore::Address> &addresses);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Path<ConstraintModel> m_path;
        QList<iscore::Address> m_addresses;

        QVector<QByteArray> m_serializedCommands;
};

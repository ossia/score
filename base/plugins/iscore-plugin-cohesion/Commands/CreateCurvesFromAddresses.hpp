#pragma once
#include <Commands/IScoreCohesionCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <State/Address.hpp>
#include <iscore/command/CommandData.hpp>
class ConstraintModel;
class CreateCurvesFromAddresses final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(IScoreCohesionCommandFactoryName(), CreateCurvesFromAddresses, "CreateCurvesFromAddresses")
    public:

        CreateCurvesFromAddresses(
            const ConstraintModel& constraint,
            const QList<iscore::Address> &addresses);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<ConstraintModel> m_path;
        QList<iscore::Address> m_addresses;

        std::vector<iscore::CommandData> m_serializedCommands;
};

#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <Process/State/MessageNode.hpp>

class InsertContentInState final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), InsertContentInState,"InsertContentInState")

    public:
       InsertContentInState(
                const QJsonObject& stateData,
                Path<StateModel>&& targetState);

        void undo() const override;

        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput& s) const override;

        void deserializeImpl(DataStreamOutput& s) override;

    private:
        MessageNode m_oldNode;
        MessageNode m_newNode;
        Path<StateModel> m_state;
};

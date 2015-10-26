#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <Commands/ScenarioCommandFactory.hpp>

#include <ProcessInterface/State/MessageNode.hpp>

class InsertContentInState : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(
                ScenarioCommandFactoryName(),
                InsertContentInState,
                "InsertContentInState")

    public:
       InsertContentInState(
                const QJsonObject& stateData,
                Path<StateModel>&& targetState);

        void undo() const override;

        void redo() const override;

    protected:
        void serializeImpl(QDataStream& s) const override;

        void deserializeImpl(QDataStream& s) override;

    private:
        MessageNode m_oldNode;
        MessageNode m_newNode;
        Path<StateModel> m_state;
};

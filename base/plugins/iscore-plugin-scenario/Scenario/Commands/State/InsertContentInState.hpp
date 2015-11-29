#pragma once
#include <Process/State/MessageNode.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <qjsonobject.h>

#include "iscore/tools/ModelPath.hpp"

class DataStreamInput;
class DataStreamOutput;
class StateModel;

class InsertContentInState final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), InsertContentInState,"Insert content in a state")

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

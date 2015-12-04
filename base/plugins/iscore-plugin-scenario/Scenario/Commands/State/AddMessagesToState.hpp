#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <QMap>

#include <Process/State/MessageNode.hpp>
#include <State/Message.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <iscore_plugin_scenario_export.h>
class DataStreamInput;
class DataStreamOutput;
class MessageItemModel;
class Process;

class ISCORE_PLUGIN_SCENARIO_EXPORT AddMessagesToState final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddMessagesToState, "Add messages to state")
        public:

          AddMessagesToState(
            Path<MessageItemModel>&&,
            const iscore::MessageList& messages);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<MessageItemModel> m_path;

        MessageNode m_oldState, m_newState;

        QMap<Id<Process>, iscore::MessageList> m_previousBackup;
        QMap<Id<Process>, iscore::MessageList> m_followingBackup;
};

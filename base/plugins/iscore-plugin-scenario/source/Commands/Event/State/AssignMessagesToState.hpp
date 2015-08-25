#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include "Document/State/StateModel.hpp"

namespace Scenario
{
namespace Command
{
class AssignMessagesToState : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL2("ScenarioControl", "AssignMessagesToState", "AssignMessagesToState")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR2(AssignMessagesToState)

          AssignMessagesToState(
          ModelPath<StateModel>&& path,
            iscore::StatePath&& statepath,
            const iscore::MessageList& messages);

        virtual void undo() override;
        virtual void redo() override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:

        ModelPath<StateModel> m_path;
        iscore::StatePath m_statepath;

        iscore::MessageList m_oldMessages; // TODO variant instead ??
        iscore::MessageList m_newMessages;
};

}
}

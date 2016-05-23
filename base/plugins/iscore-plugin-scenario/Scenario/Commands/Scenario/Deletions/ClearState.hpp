#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <State/Message.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace Scenario
{
class StateModel;

namespace Command
{
class ClearState final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), ClearState, "Clear a state")
        public:
            ClearState(Path<StateModel>&& path);
        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<StateModel> m_path;

        State::MessageList m_oldState;
};
}
}

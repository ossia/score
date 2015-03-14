#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <State/State.hpp>

class State;
namespace Scenario
{
    namespace Command
    {
        // TODO change to AddNewMessageToEventInNewState
        // TODO Add AddNewMessageToState
        class AddStateToEvent : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(AddStateToEvent, "ScenarioControl")
                AddStateToEvent(ObjectPath&& eventPath, const State& state);
                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                State m_state;
        };
    }
}

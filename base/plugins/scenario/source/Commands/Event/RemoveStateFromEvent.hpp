#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <State/State.hpp>

class State;
namespace Scenario
{
    namespace Command
    {
        class RemoveStateFromEvent : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(RemoveStateFromEvent, "ScenarioControl")
                RemoveStateFromEvent(ObjectPath&& eventPath, const State& state);
                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                State m_state;
        };

    }
}

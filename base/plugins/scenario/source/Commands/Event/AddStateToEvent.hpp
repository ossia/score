#pragma once
#include <public_interface/command/SerializableCommand.hpp>
#include <public_interface/tools/ObjectPath.hpp>

class State;
namespace Scenario
{
    namespace Command
    {
        class AddStateToEvent : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(AddStateToEvent, "ScenarioControl")
                AddStateToEvent(ObjectPath&& eventPath, QString message);
                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                QString m_message;

                id_type<State> m_stateId {};
        };
    }
}

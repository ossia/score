#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include "tools/ObjectPath.hpp"

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
                RemoveStateFromEvent(ObjectPath&& eventPath, QString message);
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
                QByteArray m_serializedState;
        };

    }
}

#pragma once
#include <core/presenter/command/SerializableCommand.hpp>

namespace Scenario
{
    namespace Command
    {
        class RemoveStateFromEvent : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(RemoveStateFromEvent, "ScenarioControl")
                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;
        };

    }
}

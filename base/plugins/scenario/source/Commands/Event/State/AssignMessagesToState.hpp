#pragma once
#include <public_interface/command/SerializableCommand.hpp>

namespace Scenario
{
    namespace Command
    {
        class AssignMessagesToState : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(AssignMessagesToState, "ScenarioControl")
                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;
        };

    }
}

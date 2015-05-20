#pragma once
#include <iscore/command/SerializableCommand.hpp>

namespace Scenario
{
    namespace Command
    {
        class SwitchStatePosition : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(SwitchStatePosition, "ScenarioControl")
                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;
        };
    }
}

#pragma once
#include <iscore/command/SerializableCommand.hpp>

namespace Scenario
{
    namespace Command
    {
        class SwitchStatePosition : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL_OBSOLETE("SwitchStatePosition", "SwitchStatePosition")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR_OBSOLETE(SwitchStatePosition, "ScenarioControl")
                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;
        };
    }
}

#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

namespace Scenario
{
    namespace Command
    {
        class SwitchStatePosition final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SwitchStatePosition, "SwitchStatePosition")
            public:
                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(QDataStream&) const override;
                void deserializeImpl(QDataStream&) override;
        };
    }
}

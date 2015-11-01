#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

namespace Scenario
{
    namespace Command
    {
        class SwitchStatePosition : public iscore::SerializableCommand
        {
                ISCORE_SERIALIZABLE_COMMAND_DECL(ScenarioCommandFactoryName(), SwitchStatePosition, "SwitchStatePosition")
            public:
                void undo() const override;
                void redo() const override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;
        };
    }
}

#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <State/Expression.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class DataStreamInput;
class DataStreamOutput;
class TimeNodeModel;

namespace Scenario
{
    namespace Command
    {
        class SetTrigger final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SetTrigger, "Change a trigger")
            public:
                SetTrigger(Path<TimeNodeModel>&& timeNodePath, State::Trigger trigger);
                ~SetTrigger();

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                Path<TimeNodeModel> m_path;
                State::Trigger m_trigger;
                State::Trigger m_previousTrigger;
        };
    }
}

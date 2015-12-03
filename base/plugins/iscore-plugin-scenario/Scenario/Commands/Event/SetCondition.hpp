#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <State/Expression.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class DataStreamInput;
class DataStreamOutput;
class EventModel;

namespace Scenario
{
    namespace Command
    {
        class SetCondition final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SetCondition, "Set an Event's condition")
            public:
                SetCondition(
                    Path<EventModel>&& eventPath,
                    iscore::Condition&& condition);
                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                Path<EventModel> m_path;
                iscore::Condition m_condition;
                iscore::Condition m_previousCondition;
        };
    }
}

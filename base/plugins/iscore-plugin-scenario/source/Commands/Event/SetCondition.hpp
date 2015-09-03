#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <State/Expression.hpp>
class EventModel;
namespace Scenario
{
    namespace Command
    {
        class SetCondition : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL_OBSOLETE("SetCondition", "SetCondition")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR_OBSOLETE(SetCondition, "ScenarioControl")
                SetCondition(
                    Path<EventModel>&& eventPath,
                    iscore::Condition&& condition);
                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<EventModel> m_path;
                iscore::Condition m_condition;
                iscore::Condition m_previousCondition;
        };
    }
}

#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class EventModel;
namespace Scenario
{
    namespace Command
    {
        class SetCondition : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("SetCondition", "SetCondition")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(SetCondition, "ScenarioControl")
                SetCondition(
                    Path<EventModel>&& eventPath,
                    QString condition);
                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<EventModel> m_path;
                QString m_condition;
                QString m_previousCondition;
        };
    }
}

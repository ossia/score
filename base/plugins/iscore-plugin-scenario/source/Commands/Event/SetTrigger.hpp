#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include "Commands/Constraint/SetRigidity.hpp"

class EventModel;
namespace Scenario
{
    namespace Command
    {
        class SetTrigger : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("SetTrigger", "SetTrigger")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(SetTrigger, "ScenarioControl")
                SetTrigger(ModelPath<EventModel>&& eventPath, QString condition);
                ~SetTrigger();

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ModelPath<EventModel> m_path;
                QString m_trigger;
                QString m_previousTrigger;

                QVector<SetRigidity*> m_cmds;
        };
    }
}

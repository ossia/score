#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

namespace Scenario
{
    namespace Command
    {
        // TODO Do a Command that can simply change a Q_PROPERTY in a generic way.
        class SetTrigger : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(SetTrigger, "ScenarioControl")
                SetTrigger(ObjectPath&& eventPath, QString condition);
                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                QString m_trigger;
                QString m_previousTrigger;
        };
    }
}

#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

namespace Scenario
{
    namespace Command
    {
        class SetCondition : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(SetCondition, "ScenarioControl")
                SetCondition(ObjectPath&& eventPath, QString condition);
                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                QString m_condition;
                QString m_previousCondition;
        };
    }
}

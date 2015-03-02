#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

class State;
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
                virtual int id() const override;
                virtual bool mergeWith(const QUndoCommand* other) override;

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

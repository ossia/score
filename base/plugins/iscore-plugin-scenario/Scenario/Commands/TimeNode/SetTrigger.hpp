#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <Scenario/Commands/Constraint/SetRigidity.hpp>

#include <State/Expression.hpp>

class TimeNodeModel;

namespace Scenario
{
    namespace Command
    {
        class SetTrigger final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SetTrigger, "SetTrigger")
            public:
                SetTrigger(Path<TimeNodeModel>&& timeNodePath, iscore::Trigger trigger);
                ~SetTrigger();

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(QDataStream&) const override;
                void deserializeImpl(QDataStream&) override;

            private:
                Path<TimeNodeModel> m_path;
                iscore::Trigger m_trigger;
                iscore::Trigger m_previousTrigger;
        };
    }
}

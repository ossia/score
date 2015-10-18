#pragma once
#include <Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include "Commands/Constraint/SetRigidity.hpp"

#include "State/Expression.hpp"

class TimeNodeModel;

namespace Scenario
{
    namespace Command
    {
        class SetTrigger : public iscore::SerializableCommand
        {
                ISCORE_SERIALIZABLE_COMMAND_DECL(ScenarioCommandFactoryName(), SetTrigger, "SetTrigger")
            public:
                SetTrigger(Path<TimeNodeModel>&& timeNodePath, iscore::Trigger trigger);
                ~SetTrigger();

                void undo() const override;
                void redo() const override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<TimeNodeModel> m_path;
                iscore::Trigger m_trigger;
                iscore::Trigger m_previousTrigger;
        };
    }
}

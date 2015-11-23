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
    class AddTrigger final : public iscore::SerializableCommand
    {
            ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddTrigger, "Add a trigger")
        public:
            AddTrigger(Path<TimeNodeModel>&& timeNodePath);
            ~AddTrigger();

            void undo() const override;
            void redo() const override;

        protected:
            void serializeImpl(DataStreamInput&) const override;
            void deserializeImpl(DataStreamOutput&) override;

        private:
            Path<TimeNodeModel> m_path;
            mutable QVector<SetRigidity*> m_cmds; // TODO investigate mutable

    };

    }
}

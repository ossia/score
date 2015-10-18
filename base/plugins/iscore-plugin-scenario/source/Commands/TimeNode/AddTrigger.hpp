#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include "Commands/Constraint/SetRigidity.hpp"

#include "State/Expression.hpp"

class TimeNodeModel;

namespace Scenario
{
    namespace Command
    {
    class AddTrigger : public iscore::SerializableCommand
    {
            ISCORE_COMMAND_DECL("ScenarioControl", "AddTrigger", "AddTrigger")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddTrigger)
            AddTrigger(Path<TimeNodeModel>&& timeNodePath);
            ~AddTrigger();

            void undo() const override;
            void redo() const override;

        protected:
            virtual void serializeImpl(QDataStream&) const override;
            virtual void deserializeImpl(QDataStream&) override;

        private:
            Path<TimeNodeModel> m_path;
            mutable QVector<SetRigidity*> m_cmds; // TODO investigate mutable

    };

    }
}

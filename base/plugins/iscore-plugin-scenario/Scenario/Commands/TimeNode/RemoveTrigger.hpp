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
    class RemoveTrigger final : public iscore::SerializableCommand
    {
        ISCORE_SERIALIZABLE_COMMAND_DECL(ScenarioCommandFactoryName(), RemoveTrigger, "RemoveTrigger")
    public:
        RemoveTrigger(Path<TimeNodeModel>&& timeNodePath);
        ~RemoveTrigger();

        void undo() const override;
        void redo() const override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        Path<TimeNodeModel> m_path;
        mutable QVector<SetRigidity*> m_cmds;

    };

    }
}

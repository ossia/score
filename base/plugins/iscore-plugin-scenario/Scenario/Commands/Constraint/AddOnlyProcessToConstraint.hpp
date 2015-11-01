#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

class ConstraintModel;
class Process;
class AddOnlyProcessToConstraint : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(
                ScenarioCommandFactoryName(),
                AddOnlyProcessToConstraint,
                "AddOnlyProcessToConstraint")
    public:
        AddOnlyProcessToConstraint(
            Path<ConstraintModel>&& constraint,
                const QString& process);
        AddOnlyProcessToConstraint(
                Path<ConstraintModel>&& constraint,
                const Id<Process>& idToUse,
                const QString& process);

        void undo() const override;
        void redo() const override;

        const Path<ConstraintModel>& constraintPath() const
        { return m_path; }

        const Id<Process>& processId() const
        { return m_createdProcessId; }

    protected:
        void serializeImpl(QDataStream& s) const override;
        void deserializeImpl(QDataStream& s) override;

    private:
        Path<ConstraintModel> m_path;
        QString m_processName;

        Id<Process> m_createdProcessId {};
};

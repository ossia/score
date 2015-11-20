#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Process/ProcessFactory.hpp>

class ConstraintModel;
class Process;
class AddOnlyProcessToConstraint final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddOnlyProcessToConstraint, "AddOnlyProcessToConstraint")
    public:
        AddOnlyProcessToConstraint(
            Path<ConstraintModel>&& constraint,
                const ProcessFactoryKey& process);
        AddOnlyProcessToConstraint(
                Path<ConstraintModel>&& constraint,
                const Id<Process>& idToUse,
                const ProcessFactoryKey& process);

        void undo() const override;
        void redo() const override;

        const Path<ConstraintModel>& constraintPath() const
        { return m_path; }

        const Id<Process>& processId() const
        { return m_createdProcessId; }

    protected:
        void serializeImpl(DataStreamInput& s) const override;
        void deserializeImpl(DataStreamOutput& s) override;

    private:
        Path<ConstraintModel> m_path;
        ProcessFactoryKey m_processName;

        Id<Process> m_createdProcessId {};
};

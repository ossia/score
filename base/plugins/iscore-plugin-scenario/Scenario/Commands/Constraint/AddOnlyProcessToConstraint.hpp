#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <Process/ProcessFactoryKey.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <iscore_plugin_scenario_export.h>
class ConstraintModel;
class DataStreamInput;
class DataStreamOutput;
class Process;

class ISCORE_PLUGIN_SCENARIO_EXPORT AddOnlyProcessToConstraint final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddOnlyProcessToConstraint, "Add a process")
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

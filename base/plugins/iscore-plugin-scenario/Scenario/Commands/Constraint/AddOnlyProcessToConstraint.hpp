#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>


#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <iscore_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;
namespace Process {
class ProcessFactory;
class ProcessModel;
}

namespace Scenario
{
class ConstraintModel;
namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT AddOnlyProcessToConstraint final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddOnlyProcessToConstraint, "Add a process")
    public:
        AddOnlyProcessToConstraint(
            Path<ConstraintModel>&& constraint,
            UuidKey<Process::ProcessFactory> process);
        AddOnlyProcessToConstraint(
            Path<ConstraintModel>&& constraint,
            Id<Process::ProcessModel> idToUse,
            UuidKey<Process::ProcessFactory> process);

        void undo() const override;
        void redo() const override;

        void undo(ConstraintModel&) const;
        Process::ProcessModel& redo(ConstraintModel&) const;

        const Path<ConstraintModel>& constraintPath() const
        { return m_path; }

        const Id<Process::ProcessModel>& processId() const
        { return m_createdProcessId; }

        const UuidKey<Process::ProcessFactory>& processKey() const
        { return m_processName; }

    protected:
        void serializeImpl(DataStreamInput& s) const override;
        void deserializeImpl(DataStreamOutput& s) override;

    private:
        Path<ConstraintModel> m_path;
        UuidKey<Process::ProcessFactory>m_processName;

        Id<Process::ProcessModel> m_createdProcessId {};
};
}
}


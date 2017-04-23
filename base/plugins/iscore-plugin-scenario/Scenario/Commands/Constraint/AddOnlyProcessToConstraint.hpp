#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>

#include <iscore_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;
namespace Process
{
class ProcessModelFactory;
class LayerFactory;
class ProcessModel;
}

namespace Scenario
{
class ConstraintModel;
namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT AddOnlyProcessToConstraint final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      AddOnlyProcessToConstraint,
      "Add a process")
public:
  AddOnlyProcessToConstraint(
      Path<ConstraintModel>&& constraint,
      UuidKey<Process::ProcessModel>
          process);
  AddOnlyProcessToConstraint(
      Path<ConstraintModel>&& constraint,
      Id<Process::ProcessModel>
          idToUse,
      UuidKey<Process::ProcessModel>
          process);

  void undo() const override;
  void redo() const override;

  void undo(ConstraintModel&) const;
  Process::ProcessModel& redo(ConstraintModel&) const;

  const Path<ConstraintModel>& constraintPath() const
  {
    return m_path;
  }

  const Id<Process::ProcessModel>& processId() const
  {
    return m_createdProcessId;
  }

  const UuidKey<Process::ProcessModel>& processKey() const
  {
    return m_processName;
  }

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ConstraintModel> m_path;
  UuidKey<Process::ProcessModel> m_processName;

  Id<Process::ProcessModel> m_createdProcessId{};
};
}
}

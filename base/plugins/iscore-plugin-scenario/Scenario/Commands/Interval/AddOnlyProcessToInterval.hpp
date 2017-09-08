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
class IntervalModel;
namespace Command
{
//! Create a new process in a interval
class ISCORE_PLUGIN_SCENARIO_EXPORT AddOnlyProcessToInterval final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      AddOnlyProcessToInterval,
      "Add a process")
public:
  AddOnlyProcessToInterval(
      const IntervalModel& cst,
      UuidKey<Process::ProcessModel> process);
  AddOnlyProcessToInterval(
      const IntervalModel& cst,
      Id<Process::ProcessModel> idToUse,
      UuidKey<Process::ProcessModel> process);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

  void undo(IntervalModel&) const;
  Process::ProcessModel& redo(IntervalModel&, const iscore::DocumentContext& ctx) const;

  const Path<IntervalModel>& intervalPath() const
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
  Path<IntervalModel> m_path;
  UuidKey<Process::ProcessModel> m_processName;

  Id<Process::ProcessModel> m_createdProcessId{};
};
}
}

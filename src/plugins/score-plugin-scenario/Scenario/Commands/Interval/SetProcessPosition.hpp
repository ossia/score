#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Process
{
class ProcessModel;
}
namespace Scenario
{
class IntervalModel;
class StateModel;
namespace Command
{
class PutProcessBefore final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), PutProcessBefore, "Set process position")

public:
  // Put proc2 before proc
  PutProcessBefore(
      const IntervalModel& cst,
      optional<Id<Process::ProcessModel>> proc,
      Id<Process::ProcessModel> proc2);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  void putBefore(
      const score::DocumentContext& ctx,
      optional<Id<Process::ProcessModel>> proc,
      Id<Process::ProcessModel> proc2) const;

  Path<Scenario::IntervalModel> m_path;
  optional<Id<Process::ProcessModel>> m_proc;
  Id<Process::ProcessModel> m_proc2;
  optional<Id<Process::ProcessModel>> m_old_after_proc2;
};

class PutStateProcessBefore final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), PutStateProcessBefore, "Set process position")

public:
  // Put proc2 before proc
  PutStateProcessBefore(
      const StateModel& cst,
      optional<Id<Process::ProcessModel>> proc,
      Id<Process::ProcessModel> proc2);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  void putBefore(
      const score::DocumentContext& ctx,
      optional<Id<Process::ProcessModel>> proc,
      Id<Process::ProcessModel> proc2) const;

  Path<Scenario::StateModel> m_path;
  optional<Id<Process::ProcessModel>> m_proc;
  Id<Process::ProcessModel> m_proc2;
  optional<Id<Process::ProcessModel>> m_old_after_proc2;
};
}
}

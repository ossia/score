#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/Slot.hpp>

#include <score/command/Command.hpp>

namespace Process
{
class ProcessModel;
}
namespace Scenario
{
class IntervalModel;
}
namespace Scenario::Command
{
class MoveProcess final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), MoveProcess, "Move a process")
public:
  MoveProcess(
      const IntervalModel& src,
      const IntervalModel& tgt,
      Id<Process::ProcessModel> processId,
      bool addSlot = true);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  const Path<IntervalModel>& source() const { return m_src; }
  const Path<IntervalModel>& target() const { return m_tgt; }
  const Id<Process::ProcessModel>& oldProcessId() const { return m_oldId; }
  const Id<Process::ProcessModel>& newProcessId() const { return m_newId; }

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<IntervalModel> m_src, m_tgt;
  Id<Process::ProcessModel> m_oldId, m_newId;
  Scenario::Rack m_oldSmall;
  Scenario::FullRack m_oldFull;
  int m_oldPos{};
  bool m_addedSlot{};
};
}

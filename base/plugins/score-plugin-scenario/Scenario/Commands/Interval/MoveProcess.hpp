#pragma once
#include <score/command/Command.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/Slot.hpp>

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
class MoveProcess final
    : public score::Command
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      MoveProcess,
      "Move a process")
public:
  MoveProcess(
      const IntervalModel& src
      , const IntervalModel& tgt
      , Id<Process::ProcessModel> processId);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

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

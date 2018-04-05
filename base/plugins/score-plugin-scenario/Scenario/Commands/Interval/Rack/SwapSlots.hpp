#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

#include <score/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class IntervalModel;
namespace Command
{
class SwapSlots final : public score::Command
{
  SCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SwapSlots, "Swap slots")
public:
  SwapSlots(Path<IntervalModel>&& rack, Slot::RackView, int pos1, int pos2);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<IntervalModel> m_path;
  Slot::RackView m_view{};
  int m_first{}, m_second{};
};
}
}

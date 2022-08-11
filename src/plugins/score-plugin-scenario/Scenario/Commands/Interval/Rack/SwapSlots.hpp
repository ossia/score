#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/Slot.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class IntervalModel;
namespace Command
{
class ChangeSlotPosition final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ChangeSlotPosition, "Change slot position")
public:
  ChangeSlotPosition(Path<IntervalModel>&& rack, Slot::RackView, int pos1, int pos2);

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

class SlotCommand : public score::Command
{
public:
  SlotCommand() = default;
  SlotCommand(const IntervalModel& c);
  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

  Path<IntervalModel> m_path;
  Scenario::Rack m_old{};
  Scenario::Rack m_new{};
};

class MoveLayerInNewSlot final : public SlotCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), MoveLayerInNewSlot, "Move layer in new slot")
public:
  MoveLayerInNewSlot(const IntervalModel&, int pos1, int pos2);
};

class MergeSlots final : public SlotCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), MergeSlots, "Merge slots")

public:
  MergeSlots(const IntervalModel&, int pos1, int pos2);
};

class MergeLayerInSlot final : public SlotCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), MergeLayerInSlot, "Merge layer")

public:
  MergeLayerInSlot(const IntervalModel&, int pos1, int pos2);
};
}
}

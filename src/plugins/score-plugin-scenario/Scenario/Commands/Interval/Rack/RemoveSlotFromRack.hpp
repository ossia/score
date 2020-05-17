#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/Slot.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

namespace Scenario
{
namespace Command
{
/**
 * @brief The RemoveSlotFromRack class
 *
 * Removes a slot. All the function views will be deleted.
 */
class RemoveSlotFromRack final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), RemoveSlotFromRack, "Remove a slot")
public:
  RemoveSlotFromRack(SlotPath slotPath, Slot slt);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  SlotPath m_path;
  Slot m_slot;
};
}
}

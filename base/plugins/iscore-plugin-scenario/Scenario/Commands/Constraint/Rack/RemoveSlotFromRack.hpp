#pragma once
#include <Scenario/Document/Constraint/Slot.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/model/Identifier.hpp>
#include <QByteArray>

namespace Scenario
{
namespace Command
{
/**
 * @brief The RemoveSlotFromRack class
 *
 * Removes a slot. All the function views will be deleted.
 */
class RemoveSlotFromRack final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), RemoveSlotFromRack, "Remove a slot")
public:
  RemoveSlotFromRack(
      SlotPath slotPath,
      Slot slt);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  SlotPath m_path;
  Slot m_slot;
};
}
}

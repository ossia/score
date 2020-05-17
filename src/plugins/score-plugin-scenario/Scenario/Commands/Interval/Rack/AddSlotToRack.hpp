#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/Slot.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

#include <score_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class IntervalModel;

namespace Command
{
/**
 * @brief The AddSlotToRack class
 *
 * Adds an empty slot at the end of a Rack.
 */
class SCORE_PLUGIN_SCENARIO_EXPORT AddSlotToRack final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), AddSlotToRack, "Create a slot")
public:
  AddSlotToRack(const Path<IntervalModel>& rackPath);
  AddSlotToRack(const Path<IntervalModel>& rackPath, Slot&& slot);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<IntervalModel> m_path;
  Scenario::Slot m_slot;
};
}
}

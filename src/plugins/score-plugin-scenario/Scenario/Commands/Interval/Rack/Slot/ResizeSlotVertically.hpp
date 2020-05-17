#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/Slot.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/Unused.hpp>

#include <score_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
namespace Command
{
/**
 * @brief The ResizeSlotVerticallyCommand class
 *
 * Changes a slot's vertical size
 */
class SCORE_PLUGIN_SCENARIO_EXPORT ResizeSlotVertically final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ResizeSlotVertically, "Resize a slot")
public:
  ResizeSlotVertically(const IntervalModel& cst, const SlotPath& slotPath, double newSize);
  ResizeSlotVertically(const IntervalModel& cst, SlotPath&& slotPath, double newSize);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void update(unused_t, unused_t, double size) { m_newSize = size; }

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  SlotPath m_path;

  double m_originalSize{};
  double m_newSize{};
};
}
}

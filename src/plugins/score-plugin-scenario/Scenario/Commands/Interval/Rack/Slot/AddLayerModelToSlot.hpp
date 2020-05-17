#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

#include <score_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;
namespace Process
{
class ProcessModel;
}

namespace Scenario
{
namespace Command
{
/**
 * @brief The AddLayerToSlot class
 *
 * Adds a process view to a slot.
 */
class SCORE_PLUGIN_SCENARIO_EXPORT AddLayerModelToSlot final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), AddLayerModelToSlot, "Add a layer to a slot")
public:
  AddLayerModelToSlot(const SlotPath& slot, Id<Process::ProcessModel> process);
  AddLayerModelToSlot(const SlotPath& slot, const Process::ProcessModel& process);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  SlotPath m_slot;
  Id<Process::ProcessModel> m_processId;
};
}
}

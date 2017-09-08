#pragma once
#include <QByteArray>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

#include <score/model/Identifier.hpp>

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
 * @brief The RemoveLayerFromSlot class
 *
 * Removes a process view from a slot.
 */
class RemoveLayerModelFromSlot final : public score::Command
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      RemoveLayerModelFromSlot,
      "Remove a layer from a slot")
public:
  RemoveLayerModelFromSlot(
      SlotPath&& slotPath, Id<Process::ProcessModel> layerId);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  SlotPath m_path;
  Id<Process::ProcessModel> m_layerId{};
};
}
}

#pragma once
#include <QByteArray>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/Identifier.hpp>

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
class RemoveLayerModelFromSlot final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      RemoveLayerModelFromSlot,
      "Remove a layer from a slot")
public:
  RemoveLayerModelFromSlot(
      SlotPath&& slotPath, Id<Process::ProcessModel> layerId);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  SlotPath m_path;
  Id<Process::ProcessModel> m_layerId{};
};
}
}

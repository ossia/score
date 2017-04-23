#pragma once
#include <QByteArray>
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
class SlotModel;
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
      Path<SlotModel>&& slotPath, Id<Process::ProcessModel> layerId);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<SlotModel> m_path;
  Id<Process::ProcessModel> m_layerId{};
};
}
}

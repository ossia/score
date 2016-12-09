#pragma once
#include <QByteArray>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/tools/SettableIdentifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace Process
{
class LayerModel;
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
      Path<SlotModel>&& slotPath, Id<Process::LayerModel> layerId);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<SlotModel> m_path;
  Id<Process::LayerModel> m_layerId{};

  QByteArray m_serializedLayerData;
};
}
}

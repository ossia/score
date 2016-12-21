#pragma once
#include <QByteArray>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace Scenario
{
class RackModel;
class SlotModel;
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
  RemoveSlotFromRack(Path<SlotModel>&& slotPath);
  RemoveSlotFromRack(Path<RackModel>&& rackPath, Id<SlotModel> slotId);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<RackModel> m_path;
  Id<SlotModel> m_slotId{};
  int m_position{};

  QByteArray m_serializedSlotData; // Should be done in the constructor
};
}
}

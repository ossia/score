#pragma once
#include <QByteArray>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_scenario_export.h>
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
class ISCORE_PLUGIN_SCENARIO_EXPORT AddLayerModelToSlot final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      AddLayerModelToSlot,
      "Add a layer to a slot")
public:
    AddLayerModelToSlot(
      const SlotPath& slot, Id<Process::ProcessModel> process);
  AddLayerModelToSlot(
      const SlotPath& slot, const Process::ProcessModel& process);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  SlotPath m_slot;
  Id<Process::ProcessModel> m_processId;
};
}
}

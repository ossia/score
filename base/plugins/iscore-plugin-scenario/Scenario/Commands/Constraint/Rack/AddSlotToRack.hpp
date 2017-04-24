#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class ConstraintModel;

namespace Command
{
/**
         * @brief The AddSlotToRack class
         *
         * Adds an empty slot at the end of a Rack.
         */
class ISCORE_PLUGIN_SCENARIO_EXPORT AddSlotToRack final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), AddSlotToRack, "Create a slot")
public:
  AddSlotToRack(const Path<ConstraintModel>& rackPath);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ConstraintModel> m_path;

};
}
}

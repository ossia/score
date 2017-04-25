#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore_plugin_scenario_export.h>
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
class ISCORE_PLUGIN_SCENARIO_EXPORT ResizeSlotVertically final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), ResizeSlotVertically, "Resize a slot")
public:
    ResizeSlotVertically(const SlotPath& slotPath, double newSize);
  ResizeSlotVertically(SlotPath&& slotPath, double newSize);

  void undo() const override;
  void redo() const override;

  void update(unused_t, double size)
  {
    m_newSize = size;
  }

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

#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_scenario_export.h>
namespace Scenario
{
class ConstraintModel;
namespace Command
{
/**
 * @brief The ShowRackInViewModel class
 *
 * For a given constraint view model,
 * select the rack that is to be shown, and show it.
 */
class ISCORE_PLUGIN_SCENARIO_EXPORT ShowRack final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), ShowRack, "Show a rack")
public:
  ShowRack(const Scenario::ConstraintModel& vm);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ConstraintModel> m_constraintViewPath;
};
}
}

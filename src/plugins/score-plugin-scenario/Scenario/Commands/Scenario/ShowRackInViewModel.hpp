#pragma once
#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <score_plugin_scenario_export.h>
namespace Scenario
{
class IntervalModel;
namespace Command
{
/**
 * @brief The ShowRackInViewModel class
 *
 * For a given interval view model,
 * select the rack that is to be shown, and show it.
 */
class SCORE_PLUGIN_SCENARIO_EXPORT ShowRack final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ShowRack, "Show a rack")
public:
  ShowRack(const Scenario::IntervalModel& vm);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<IntervalModel> m_intervalViewPath;
  bool m_old{};
};
}
}

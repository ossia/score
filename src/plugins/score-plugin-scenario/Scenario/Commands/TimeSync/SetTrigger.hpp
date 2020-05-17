#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <State/Expression.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class TimeSyncModel;
namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT SetTrigger final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetTrigger, "Change a trigger")
public:
  SetTrigger(const TimeSyncModel& tn, State::Expression trigger);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<TimeSyncModel> m_path;
  State::Expression m_trigger;
  State::Expression m_previousTrigger;
};
}
}

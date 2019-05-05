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
class SCORE_PLUGIN_SCENARIO_EXPORT SetAutoTrigger final : public score::Command
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      SetAutoTrigger,
      "Change a trigger")
public:
  SetAutoTrigger(const TimeSyncModel& tn, bool t);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<TimeSyncModel> m_path;
  bool m_old{}, m_new{};
};
}
}

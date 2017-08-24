#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <State/Expression.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class TimeSyncModel;
namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT SetTrigger final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), SetTrigger, "Change a trigger")
public:
  SetTrigger(
      const TimeSyncModel& tn,
      State::Expression trigger);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

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

#pragma once
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <score/command/PropertyCommand.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <State/Expression.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

namespace Scenario
{
class TimeSyncModel;
namespace Command
{
using TimeSyncModel = ::Scenario::TimeSyncModel;
class SCORE_PLUGIN_SCENARIO_EXPORT SetAutoTrigger final : public score::Command
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
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

PROPERTY_COMMAND_T(Scenario::Command, SetTimeSyncMusicalSync, TimeSyncModel::p_musicalSync, "Set sync")
SCORE_COMMAND_DECL_T(Scenario::Command::SetTimeSyncMusicalSync)

PROPERTY_COMMAND_T(Scenario::Command, SetTimeSyncIsStartPoint, TimeSyncModel::p_startPoint, "Set start point")
SCORE_COMMAND_DECL_T(Scenario::Command::SetTimeSyncIsStartPoint)

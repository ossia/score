#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Sequence/SequenceModel.hpp>

#include <Process/TimeValue.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Scenario
{
class TimeSyncModel;
}

namespace Sequence
{
namespace Command
{

class MoveSequenceIS final : public score::Command
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(), MoveSequenceIS, "Move sequence IS")
public:
  MoveSequenceIS(
      const SequenceModel& seq, const Id<Scenario::TimeSyncModel>& tsId, TimeVal newDate);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void update(
      const SequenceModel& seq, const Id<Scenario::TimeSyncModel>& tsId, TimeVal newDate);

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<SequenceModel> m_path;
  Id<Scenario::TimeSyncModel> m_tsId;
  TimeVal m_oldDate;
  TimeVal m_newDate;
};

}
}

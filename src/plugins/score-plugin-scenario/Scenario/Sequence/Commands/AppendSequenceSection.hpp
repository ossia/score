#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Sequence/SequenceModel.hpp>

#include <Process/TimeValue.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Sequence
{
namespace Command
{

class AppendSequenceSection final : public score::Command
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(), AppendSequenceSection,
      "Append sequence section")
public:
  AppendSequenceSection(const SequenceModel& seq, TimeVal duration);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<SequenceModel> m_path;
  TimeVal m_duration;
  mutable SequenceModel::AppendedSection m_info;
  mutable bool m_firstRedo{true};
};

}
}

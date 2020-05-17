#pragma once

#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/Unused.hpp>
#include <score/tools/std/Optional.hpp>

namespace Scenario
{
class CommentBlockModel;
namespace Command
{
class MoveCommentBlock final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), MoveCommentBlock, "Move a comment block")
public:
  MoveCommentBlock(
      const ProcessModel& scenarPath,
      Id<CommentBlockModel> id,
      TimeVal newDate,
      double newY);

  void update(unused_t, unused_t, TimeVal newDate, double newYPos)
  {
    m_newDate = std::move(newDate);
    m_newY = newYPos;
  }
  // Command interface

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ProcessModel> m_path;
  Id<CommentBlockModel> m_id;
  TimeVal m_oldDate, m_newDate;
  double m_oldY{}, m_newY{};
};
}
}

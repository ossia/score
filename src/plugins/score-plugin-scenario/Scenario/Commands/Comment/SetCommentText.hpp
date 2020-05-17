#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

namespace Scenario
{
class CommentBlockModel;
namespace Command
{
//! Changes the comment in a comment block
class SetCommentText final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetCommentText, "Set Text in comment block")
public:
  SetCommentText(const CommentBlockModel& model, QString newComment);

private:
  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

  Path<CommentBlockModel> m_path;
  QString m_newComment;
  QString m_oldComment;
};
}
}

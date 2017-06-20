#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>

#include <QTextDocument>

namespace Scenario
{
class CommentBlockModel;
namespace Command
{
//! Changes the comment in a comment block
class SetCommentText final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), SetCommentText,
      "Set Text in comment block")
public:
  SetCommentText(const CommentBlockModel& model, QString newComment);

private:
  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

  Path<CommentBlockModel> m_path;
  QString m_newComment;
  QString m_oldComment;
};
}
}

#pragma once

#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

namespace Scenario
{
class CommentBlockModel;

namespace Command
{
class CreateCommentBlock final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), CreateCommentBlock, "Create a comment block")
public:
  CreateCommentBlock(const Scenario::ProcessModel& scenarioPath, TimeVal date, double yPosition);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ProcessModel> m_path;
  TimeVal m_date;
  double m_y{};

  Id<CommentBlockModel> m_id;
};
class RemoveCommentBlock final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), RemoveCommentBlock, "Remove a comment block")
public:
  RemoveCommentBlock(const Scenario::ProcessModel& sc, const Scenario::CommentBlockModel& cb);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ProcessModel> m_path;
  Id<CommentBlockModel> m_id;
  QByteArray m_block;
};
}
}

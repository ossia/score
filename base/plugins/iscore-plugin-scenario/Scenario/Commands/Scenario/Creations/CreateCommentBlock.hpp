#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <iscore/command/Command.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>

#include <Process/TimeValue.hpp>

namespace Scenario
{
class CommentBlockModel;

namespace Command
{
class CreateCommentBlock final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      CreateCommentBlock,
      "Create a comment block")
public:
  CreateCommentBlock(
      const Scenario::ProcessModel& scenarioPath,
      TimeVal date,
      double yPosition);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ProcessModel> m_path;
  TimeVal m_date;
  double m_y{};

  Id<CommentBlockModel> m_id;
};
}
}

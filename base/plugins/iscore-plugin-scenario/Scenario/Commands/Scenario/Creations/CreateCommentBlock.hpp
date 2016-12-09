#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <iscore/command/Command.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

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
      Path<ProcessModel>&& scenarioPath, TimeValue date, double yPosition);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ProcessModel> m_path;
  TimeValue m_date;
  double m_y;

  Id<CommentBlockModel> m_id;
};
}
}

#pragma once
#include <Process/State/MessageNode.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class StateModel;

namespace Command
{
class RemoveMessageNodes final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), RemoveMessageNodes, "Remove user messages")

public:
  RemoveMessageNodes(
      const Scenario::StateModel& model,
      const std::vector<const Process::MessageNode*>&);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<StateModel> m_path;
  Process::MessageNode m_oldState;
  Process::MessageNode m_newState;
};
}
}

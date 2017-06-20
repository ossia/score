#pragma once
#include <QList>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>

#include <Process/State/MessageNode.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class StateModel;

namespace Command
{
class RemoveMessageNodes final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), RemoveMessageNodes, "Remove user messages")

public:
  RemoveMessageNodes(
      const Scenario::StateModel& model,
      const std::vector<const Process::MessageNode*>&);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

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

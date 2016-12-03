#pragma once
#include <QList>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <Process/State/MessageNode.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class StateModel;

namespace Command
{
class RemoveMessageNodes final : public iscore::SerializableCommand
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), RemoveMessageNodes, "Remove user messages")

public:
  RemoveMessageNodes(
      Path<StateModel>&&, const QList<const Process::MessageNode*>&);

  void undo() const override;
  void redo() const override;

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

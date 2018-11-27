#pragma once
#include <Process/State/MessageNode.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

namespace Scenario
{
class StateModel;
namespace Command
{

class InsertContentInState final : public score::Command
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), InsertContentInState,
      "Insert content in a state")

public:
  InsertContentInState(
      const QJsonObject& stateData, const Scenario::StateModel& state);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

private:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;
  State::MessageList m_oldNode;
  State::MessageList m_newNode;
  Path<StateModel> m_state;
};
}
}

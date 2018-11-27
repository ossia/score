#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <State/Message.hpp>
#include <Process/State/MessageNode.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace Scenario
{
class StateModel;

namespace Command
{
class ClearState final : public score::Command
{
  SCORE_COMMAND_DECL(ScenarioCommandFactoryName(), ClearState, "Clear a state")
public:
  ClearState(const StateModel& path);
  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<StateModel> m_path;

  Process::MessageNode m_oldState;
};
}
}

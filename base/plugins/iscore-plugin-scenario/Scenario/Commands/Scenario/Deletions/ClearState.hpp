#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <State/Message.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace Scenario
{
class StateModel;

namespace Command
{
class ClearState final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), ClearState, "Clear a state")
public:
  ClearState(const StateModel& path);
  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<StateModel> m_path;

  State::MessageList m_oldState;
};
}
}

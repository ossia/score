#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <State/Expression.hpp>
#include <iscore/command/PropertyCommand.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/tools/ModelPath.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class EventModel;
namespace Command
{
class SetCondition final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), SetCondition, "Set an Event's condition")
public:
  SetCondition(Path<EventModel>&& eventPath, State::Expression&& condition);
  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<EventModel> m_path;
  State::Expression m_condition;
  State::Expression m_previousCondition;
};

class SetOffsetBehavior final : public iscore::PropertyCommand
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), SetOffsetBehavior, "Set offset behavior")
public:
  SetOffsetBehavior(Path<EventModel>&& path, OffsetBehavior newval);
};
}
}

#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <State/Expression.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class TimeNodeModel;
namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT SetTrigger final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), SetTrigger, "Change a trigger")
public:
  SetTrigger(Path<TimeNodeModel>&& timeNodePath, State::Expression trigger);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<TimeNodeModel> m_path;
  State::Expression m_trigger;
  State::Expression m_previousTrigger;
};
}
}

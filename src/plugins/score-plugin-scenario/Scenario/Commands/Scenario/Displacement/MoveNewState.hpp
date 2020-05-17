#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

struct DataStreamInput;
struct DataStreamOutput;

/*
 * Used on creation mode, when mouse is pressed and is moving.
 * In this case, only vertical move is allowed (new state on an existing event)
 */
namespace Scenario
{
class StateModel;
class ProcessModel;

namespace Command
{

class SCORE_PLUGIN_SCENARIO_EXPORT MoveNewState final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), MoveNewState, "Move a new state")
public:
  MoveNewState(const Scenario::ProcessModel& scenar, Id<StateModel> stateId, double y);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void update(const Path<Scenario::ProcessModel>&, const Id<StateModel>&, double y) { m_y = y; }

  const Path<Scenario::ProcessModel>& path() const { return m_path; }

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::ProcessModel> m_path;
  Id<StateModel> m_stateId;
  double m_y{}, m_oldy{};
};
}
}

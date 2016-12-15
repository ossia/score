#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/std/Optional.hpp>

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

class MoveNewState final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), MoveNewState, "Move a new state")
public:
  MoveNewState(
      Path<Scenario::ProcessModel>&& scenarioPath,
      Id<StateModel>
          stateId,
      double y);

  void undo() const override;
  void redo() const override;

  void
  update(const Path<Scenario::ProcessModel>&, const Id<StateModel>&, double y)
  {
    m_y = y;
  }

  const Path<Scenario::ProcessModel>& path() const
  {
    return m_path;
  }

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

#pragma once
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

/*
 * Command for vertical move so it does'nt have to resize anything on time axis
 * */
namespace Scenario
{
class ConstraintModel;
class ProcessModel;
namespace Command
{
class MoveConstraint final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), MoveConstraint, "Move a constraint")
public:
  MoveConstraint(
      Path<Scenario::ProcessModel>&& scenarioPath,
      Id<ConstraintModel>
          id,
      double y);

  void update(unused_t, unused_t, double height)
  {
    m_newHeight = height;
  }

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::ProcessModel> m_path;
  Id<ConstraintModel> m_constraint;
  double m_oldHeight{};
  double m_newHeight{};

  QList<QPair<Id<ConstraintModel>, double>> m_selectedConstraints;
};
}
}

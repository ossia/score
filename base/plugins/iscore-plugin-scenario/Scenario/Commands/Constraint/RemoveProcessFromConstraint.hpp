#pragma once
#include <QByteArray>
#include <QPair>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <QVector>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace Process
{
class ProcessModel;
}

namespace Scenario
{
class ConstraintModel;
namespace Command
{
class RemoveProcessFromConstraint final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      RemoveProcessFromConstraint,
      "Remove a process")
public:
  RemoveProcessFromConstraint(
      Path<ConstraintModel>&& constraintPath,
      Id<Process::ProcessModel>
          processId);
  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ConstraintModel> m_path;
  Id<Process::ProcessModel> m_processId;
  QByteArray m_serializedProcessData;

  Rack m_smallView;
};
}
}

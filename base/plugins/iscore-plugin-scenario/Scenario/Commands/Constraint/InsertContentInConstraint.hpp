#pragma once
#include <Process/ExpandMode.hpp>
#include <QJsonObject>
#include <QMap>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>

#include <iscore/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace Process
{
class ProcessModel;
}
namespace Scenario
{
class RackModel;
class ConstraintModel;

namespace Command
{
class InsertContentInConstraint final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      InsertContentInConstraint,
      "Insert content in a constraint")
public:
  InsertContentInConstraint(
      QJsonObject&& sourceConstraint,
      Path<ConstraintModel>&& targetConstraint,
      ExpandMode mode);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  QJsonObject m_source;
  Path<ConstraintModel> m_target;
  ExpandMode m_mode{ExpandMode::GrowShrink};

  QMap<Id<Process::ProcessModel>, Id<Process::ProcessModel>> m_processIds;
};
}
}

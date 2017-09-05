#pragma once
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <QByteArray>
#include <QObject>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Slot.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/command/AggregateCommand.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <QString>
#include <vector>


#include <iscore/application/ApplicationContext.hpp>

#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#include <Scenario/Commands/Constraint/AddOnlyProcessToConstraint.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
namespace Command
{
class AddProcessToConstraint final : public iscore::Command
{
    ISCORE_COMMAND_DECL(
        ScenarioCommandFactoryName(),
        AddProcessToConstraint,
        "Add a process to a constraint")

  public:
  AddProcessToConstraint(
      const ConstraintModel& constraint,
      const UuidKey<Process::ProcessModel>& process);
    ~AddProcessToConstraint();

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

  const Path<ConstraintModel>& constraintPath() const;
  const Id<Process::ProcessModel>& processId() const;
  const UuidKey<Process::ProcessModel>& processKey() const;

private:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

  AddOnlyProcessToConstraint m_addProcessCommand;
  bool m_addedSlot{};
};

class AddProcessInNewBoxMacro final : public iscore::AggregateCommand
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), AddProcessInNewBoxMacro,
      "Add a process in a new box")

  public:
    ~AddProcessInNewBoxMacro();
};
}
}


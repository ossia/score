#pragma once
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <QByteArray>
#include <QObject>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
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

#include <Process/LayerModel.hpp>

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
class AddProcessToConstraintBase : public iscore::Command
{
public:
  AddProcessToConstraintBase() = default;
  AddProcessToConstraintBase(
      const ConstraintModel& constraint,
      UuidKey<Process::ProcessModel> process)
      : m_addProcessCommand{constraint,
                            getStrongId(constraint.processes),
                            process}
  {
  }

  const Path<ConstraintModel>& constraintPath() const
  {
    return m_addProcessCommand.constraintPath();
  }
  const Id<Process::ProcessModel>& processId() const
  {
    return m_addProcessCommand.processId();
  }
  const UuidKey<Process::ProcessModel>& processKey() const
  {
    return m_addProcessCommand.processKey();
  }

protected:
  AddOnlyProcessToConstraint m_addProcessCommand;
};

template <typename AddProcessDelegate>
class AddProcessToConstraint final : public AddProcessToConstraintBase
{
  friend AddProcessDelegate;

public:
  const CommandGroupKey& parentKey() const noexcept override
  {
    return ScenarioCommandFactoryName();
  }
  const CommandKey& key() const noexcept override
  {
    return static_key();
  }
  QString description() const override
  {
    return QObject::tr("Add a process to a constraint");
  }
  static const CommandKey& static_key() noexcept
  {
    return AddProcessDelegate::static_key();
  }

  AddProcessToConstraint() = default;

  AddProcessToConstraint(
      const ConstraintModel& constraint,
      const UuidKey<Process::ProcessModel>& process)
      : AddProcessToConstraintBase{constraint, process}
  {
    auto& fact = context.interfaces<Process::LayerFactoryList>();
    m_delegate.init(fact, constraint);
  }

  void undo() const override
  {
    auto& constraint = m_addProcessCommand.constraintPath().find();

    m_delegate.undo(constraint);
    m_addProcessCommand.undo(constraint);
  }
  void redo() const override
  {
    auto& constraint = m_addProcessCommand.constraintPath().find();

    // Create process model
    auto& proc = m_addProcessCommand.redo(constraint);
    m_delegate.redo(constraint, proc);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_addProcessCommand.serialize();
    m_delegate.serialize(s);
  }
  void deserializeImpl(DataStreamOutput& s) override
  {
    QByteArray b;
    s >> b;

    m_addProcessCommand.deserialize(b);
    m_delegate.deserialize(s);
  }

private:
  AddProcessDelegate m_delegate{*this};
};

class NotBaseConstraint
{
};
class IsBaseConstraint
{
};
class HasNoSlots
{
};
class HasSlots
{
};

template <typename... Traits>
class AddProcessDelegate;

template <>
class AddProcessDelegate<HasNoSlots, NotBaseConstraint>
{
private:
  using proc_t
      = AddProcessToConstraint<AddProcessDelegate<HasNoSlots, NotBaseConstraint>>;
  proc_t& m_cmd;

public:
  static const CommandKey& static_key()
  {
    static const CommandKey var{
        "AddProcessDelegate_HasNoSlots_HasRacks_NotBaseConstraint"};
    return var;
  }

  AddProcessDelegate<HasNoSlots, NotBaseConstraint>(proc_t& cmd)
      : m_cmd{cmd}
  {
  }

  void init(
      const Process::LayerFactoryList& fact, const ConstraintModel& constraint)
  {
  }

  void undo(ConstraintModel& constraint) const
  {
    constraint.removeSlot(SlotId{constraint.smallView().size() - 1, Slot::SmallView});
  }

  void redo(ConstraintModel& constraint, Process::ProcessModel& proc) const
  {
    auto h
        = m_cmd.context.settings<Scenario::Settings::Model>().getSlotHeight();
    constraint.addSlot(Slot{{proc.id()}, proc.id(), h});
  }

  void serialize(DataStreamInput& s) const
  {
  }

  void deserialize(DataStreamOutput& s)
  {
  }

private:
};

template <>
class AddProcessDelegate<HasSlots, NotBaseConstraint>
{
private:
  using proc_t
      = AddProcessToConstraint<AddProcessDelegate<HasSlots, NotBaseConstraint>>;
  proc_t& m_cmd;

public:
  static const CommandKey& static_key()
  {
    static const CommandKey var{
        "AddProcessDelegate_HasSlots_HasRacks_NotBaseConstraint"};
    return var;
  }

  AddProcessDelegate<HasSlots, NotBaseConstraint>(proc_t& cmd)
      : m_cmd{cmd}
  {
  }

  void init(
      const Process::LayerFactoryList& fact,
      const ConstraintModel& constraint)
  {
  }

  void undo(ConstraintModel& constraint) const
  {
    constraint.removeLayer(SlotId{0, Slot::SmallView}, m_cmd.processId());
  }

  void redo(ConstraintModel& constraint, Process::ProcessModel& proc) const
  {
    constraint.addLayer(SlotId{0, Slot::SmallView}, m_cmd.processId());
  }

  void serialize(DataStreamInput& s) const
  {
  }

  void deserialize(DataStreamOutput& s)
  {
  }

};

template <>
class AddProcessDelegate<IsBaseConstraint>
{

  using proc_t
      = AddProcessToConstraint<AddProcessDelegate<IsBaseConstraint>>;

public:
  static const CommandKey& static_key()
  {
    static const CommandKey var{
        "AddProcessDelegateWhenRacksAndBaseConstraint"};
    return var;
  }

  AddProcessDelegate<IsBaseConstraint>(const proc_t&)
  {
  }

  void init(
      const Process::LayerFactoryList& fact, const ConstraintModel& constraint)
  {
    // Base constraint : add in new slot?
  }

  void undo(ConstraintModel& constraint) const
  {
  }

  void redo(ConstraintModel& constraint, Process::ProcessModel& proc) const
  {
  }

  void serialize(DataStreamInput& s) const
  {
  }

  void deserialize(DataStreamOutput& s)
  {
  }
};

inline Scenario::Command::AddProcessToConstraintBase*
make_AddProcessToConstraint(
    const ConstraintModel& constraint,
    const UuidKey<Process::ProcessModel>& process)
{
  auto isScenarioModel
      = dynamic_cast<Scenario::ProcessModel*>(constraint.parent());

  Scenario::Command::AddProcessToConstraintBase* cmd{};

  auto noSlots = constraint.smallView().empty();
  if (isScenarioModel)
  {
    if (noSlots)
    {
      cmd = new AddProcessToConstraint<AddProcessDelegate<HasNoSlots, NotBaseConstraint>>{
        constraint, process};
    }
    else
    {
      cmd = new AddProcessToConstraint<AddProcessDelegate<HasSlots, NotBaseConstraint>>{
        constraint, process};
    }
  }
  else
  {
    if (noSlots)
    {
      ISCORE_TODO;
    }
    else
    {
      cmd = new AddProcessToConstraint<AddProcessDelegate<IsBaseConstraint>>{
        constraint, process};
    }
  }

  return cmd;
}

class AddProcessInNewBoxMacro final : public iscore::AggregateCommand
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), AddProcessInNewBoxMacro,
      "Add a process in a new box")
};

// To make the preprocessor happy
using AddProcessDelegate_HasNoSlots_HasRacks_NotBaseConstraint
    = AddProcessDelegate<HasNoSlots, NotBaseConstraint>;
using AddProcessDelegate_HasSlots_HasRacks_NotBaseConstraint
    = AddProcessDelegate<HasSlots, NotBaseConstraint>;
using AddProcessDelegate_HasRacks_BaseConstraint
    = AddProcessDelegate<IsBaseConstraint>;
}
}

ISCORE_COMMAND_DECL_T(
    AddProcessToConstraint<AddProcessDelegate_HasNoSlots_HasRacks_NotBaseConstraint>)
ISCORE_COMMAND_DECL_T(
    AddProcessToConstraint<AddProcessDelegate_HasSlots_HasRacks_NotBaseConstraint>)
ISCORE_COMMAND_DECL_T(
    AddProcessToConstraint<AddProcessDelegate_HasRacks_BaseConstraint>)

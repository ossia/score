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
      const iscore::ApplicationContext& context,
      const ConstraintModel& constraint,
      const UuidKey<Process::ProcessModel>& process)
      : AddProcessToConstraintBase{constraint, process}
  {
    auto& fact = context.interfaces<Process::LayerFactoryList>();
    m_delegate.init(fact, constraint);
  }

  void undo(const iscore::DocumentContext& ctx) const override
  {
    auto& constraint = m_addProcessCommand.constraintPath().find(ctx);

    m_delegate.undo(constraint);
    m_addProcessCommand.undo(ctx);
  }
  void redo(const iscore::DocumentContext& ctx) const override
  {
    auto& constraint = m_addProcessCommand.constraintPath().find(ctx);

    // Create process model
    auto& proc = m_addProcessCommand.redo(constraint, ctx);
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

class HasNoSlots
{
};
class HasSlots
{
};

template <typename t>
class AddProcessDelegate;

template <>
class AddProcessDelegate<HasNoSlots>
{
private:
  using proc_t
      = AddProcessToConstraint<AddProcessDelegate<HasNoSlots>>;
  proc_t& m_cmd;

public:
  static const CommandKey& static_key()
  {
    static const CommandKey var{
        "AddProcessDelegate_HasNoSlots_HasRacks_NotBaseConstraint"};
    return var;
  }

  AddProcessDelegate<HasNoSlots>(proc_t& cmd)
      : m_cmd{cmd}
  {
  }

  void init(
      const Process::LayerFactoryList& fact, const ConstraintModel& constraint)
  {
  }

  void undo(ConstraintModel& constraint) const
  {
    constraint.removeSlot(int(constraint.smallView().size() - 1));
  }

  void redo(ConstraintModel& constraint, Process::ProcessModel& proc) const
  {
    auto h
        = iscore::AppContext().settings<Scenario::Settings::Model>().getSlotHeight();
    constraint.addSlot(Slot{{proc.id()}, proc.id(), h});
    constraint.setSmallViewVisible(true);
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
class AddProcessDelegate<HasSlots>
{
private:
  using proc_t
      = AddProcessToConstraint<AddProcessDelegate<HasSlots>>;
  proc_t& m_cmd;

public:
  static const CommandKey& static_key()
  {
    static const CommandKey var{
        "AddProcessDelegate_HasSlots_HasRacks_NotBaseConstraint"};
    return var;
  }

  AddProcessDelegate<HasSlots>(proc_t& cmd)
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
    constraint.removeLayer(0, m_cmd.processId());
  }

  void redo(ConstraintModel& constraint, Process::ProcessModel& proc) const
  {
    constraint.addLayer(0, m_cmd.processId());
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
  Scenario::Command::AddProcessToConstraintBase* cmd{};

  auto noSlots = constraint.smallView().empty();
  if (noSlots)
  {
    cmd = new AddProcessToConstraint<AddProcessDelegate<HasNoSlots>>{iscore::AppContext(),
                                                                    constraint, process};
  }
  else
  {
    cmd = new AddProcessToConstraint<AddProcessDelegate<HasSlots>>{iscore::AppContext(),
                                                                  constraint, process};
  }

  return cmd;
}

class AddProcessInNewBoxMacro final : public iscore::AggregateCommand
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), AddProcessInNewBoxMacro,
      "Add a process in a new box")
};
}
}

ISCORE_COMMAND_DECL_T(
    AddProcessToConstraint<AddProcessDelegate<HasNoSlots>>)
ISCORE_COMMAND_DECL_T(
    AddProcessToConstraint<AddProcessDelegate<HasSlots>>)

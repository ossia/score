#pragma once
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>
#include <Scenario/Tools/dataStructures.hpp>

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

/*
 * Command to change a constraint duration by moving event. Vertical move is
 * not allowed.
 */
namespace Scenario
{
class BaseScenario;
namespace Command
{
template <typename SimpleScenario_T>
class MoveBaseEvent final : public iscore::SerializableCommand
{
private:
  template <typename ScaleFun>
  static void updateDuration(
      SimpleScenario_T& scenar,
      const TimeValue& newDuration,
      ScaleFun&& scaleMethod)
  {
    scenar.endEvent().setDate(newDuration);
    scenar.endTimeNode().setDate(newDuration);

    auto& constraint = scenar.constraint();
    ConstraintDurations::Algorithms::changeAllDurations(
        constraint, newDuration);
    for (auto& process : constraint.processes)
    {
      scaleMethod(process, newDuration);
    }
  }

public:
  const CommandParentFactoryKey& parentKey() const override
  {
    return CommandFactoryName<SimpleScenario_T>();
  }
  const CommandFactoryKey& key() const override
  {
    return static_key();
  }
  QString description() const override
  {
    return QObject::tr("Move a %1 event")
        .arg(Metadata<UndoName_k, SimpleScenario_T>::get());
  }
  static const CommandFactoryKey& static_key()
  {
    static const CommandFactoryKey kagi{
        QString("MoveBaseEvent_")
        + Metadata<ObjectKey_k, SimpleScenario_T>::get()};
    return kagi;
  }

  MoveBaseEvent() = default;

  MoveBaseEvent(
      Path<SimpleScenario_T>&& path,
      const Id<EventModel>& event,
      const TimeValue& date,
      double y,
      ExpandMode mode)
      : m_path{std::move(path)}, m_newDate{date}, m_mode{mode}
  {
    auto& scenar = m_path.find();
    const auto& constraint = scenar.constraint();
    m_oldDate = constraint.duration.defaultDuration();
    m_saveData = ConstraintSaveData{constraint};
  }

  void undo() const override
  {
    auto& scenar = m_path.find();

    updateDuration(
        scenar, m_oldDate, [&](Process::ProcessModel& p, const TimeValue& v) {
          // Nothing is needed since the processes will be replaced anyway.
        });

    // TODO do this only if we shrink.

    // Now we have to restore the state of each constraint that might have been
    // modified
    // during this command.

    // 1. Clear the constraint
    ClearConstraint clearCmd{scenar.constraint()};
    clearCmd.redo();

    // 2. Restore
    auto& constraint = scenar.constraint();
    m_saveData.reload(constraint);
  }

  void redo() const override
  {
    auto& scenar = m_path.find();

    updateDuration(
        scenar, m_newDate, [&](Process::ProcessModel& p, const TimeValue& v) {
          p.setParentDuration(m_mode, v);
        });
  }

  void update(unused_t, unused_t, const TimeValue& date, double, ExpandMode)
  {
    m_newDate = date;
  }

  const Path<SimpleScenario_T>& path() const
  {
    return m_path;
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_path << m_oldDate << m_newDate << (int)m_mode << m_saveData;
  }
  void deserializeImpl(DataStreamOutput& s) override
  {
    int mode;
    s >> m_path >> m_oldDate >> m_newDate >> mode >> m_saveData;
    m_mode = static_cast<ExpandMode>(mode);
  }

private:
  Path<SimpleScenario_T> m_path;

  TimeValue m_oldDate{};
  TimeValue m_newDate{};

  ExpandMode m_mode{ExpandMode::Scale};

  ConstraintSaveData m_saveData;
};
}
}

ISCORE_COMMAND_DECL_T(MoveBaseEvent<Scenario::BaseScenario>)

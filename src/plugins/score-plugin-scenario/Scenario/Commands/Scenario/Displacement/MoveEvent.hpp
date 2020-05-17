#pragma once
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Scenario/Displacement/SerializableMoveEvent.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>

#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Tools/dataStructures.hpp>
#include <Scenario/Tools/elementFindingHelper.hpp>

#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

struct ElementsProperties;

#include <tests/helpers/ForwardDeclaration.hpp>

namespace Scenario
{
class EventModel;
class TimeSyncModel;
class IntervalModel;
class ProcessModel;
namespace Command
{
/**
 * This class use the new Displacement policy class
 */
template <class DisplacementPolicy>
class MoveEvent final : public SerializableMoveEvent
{
  // No SCORE_COMMAND here since it's a template.

public:
  const CommandGroupKey& parentKey() const noexcept override
  {
    return CommandFactoryName();
  }
  const CommandKey& key() const noexcept override
  {
    static const QByteArray name
        = QString{"MoveEvent_%1"}.arg(DisplacementPolicy::name()).toLatin1();
    static const CommandKey kagi{name.constData()};
    return kagi;
  }
  QString description() const override
  {
    return QObject::tr("Move an event with %1")
        .arg(DisplacementPolicy::name());
  }

  MoveEvent() : SerializableMoveEvent{} {}
  /**
   * @brief MoveEvent
   * @param scenarioPath
   * @param eventId
   * @param newDate
   * !!! in the future it would be better to give directly the delta
   * time of the mouse displacement  !!!
   * @param mode
   */
  MoveEvent(
      const Scenario::ProcessModel& scenario,
      const Id<EventModel>& eventId,
      const TimeVal& newDate,
      ExpandMode mode,
      LockMode lock)
      : SerializableMoveEvent{}, m_path{scenario}, m_mode{mode}, m_lock{lock}
  {
    auto& s = const_cast<Scenario::ProcessModel&>(scenario);
    DisplacementPolicy::init(s, {scenario.event(eventId).timeSync()});
    // we need to compute the new time delta and store this initial event id
    // for recalculate the delta on updates
    // NOTE: in the future in would be better to give directly the delta value
    // to this method ?,
    // in that way we wouldn't need to keep the initial event and recalculate
    // the delta
    m_eventId = eventId;
    m_initialDate = getDate(scenario, eventId);

    update(s, eventId, newDate, 0, m_mode, lock);
  }

  void update(
      Scenario::ProcessModel& scenario,
      const Id<EventModel>& eventId,
      const TimeVal& newDate,
      double,
      ExpandMode,
      LockMode) override
  {
    // we need to compute the new time delta
    // NOTE: in the future in would be better to give directly the delta value
    // to this method
    TimeVal deltaDate = newDate - m_initialDate;

    // NOTICE: multiple event displacement functionnality already available,
    // this is "retro" compatibility
    QVector<Id<TimeSyncModel>> draggedElements;
    draggedElements.push_back(
        scenario.events.at(eventId).timeSync()); // retrieve corresponding
                                                 // timesync and store it in
                                                 // array

    // the displacement is computed here and we don't need to know how.
    DisplacementPolicy::computeDisplacement(
        scenario, draggedElements, deltaDate, m_savedElementsProperties);
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    auto& scenario = m_path.find(ctx);

    // update positions using old stored dates
    DisplacementPolicy::revertPositions(
        ctx,
        scenario,
        [&](Process::ProcessModel& p, const TimeVal& t) {
          p.setParentDuration(m_mode, t);
        },
        m_savedElementsProperties);

    // Dataflow::restoreCables(m_savedElementsProperties.cables, ctx);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    auto& scenario = m_path.find(ctx);

    // update positions using new stored dates
    DisplacementPolicy::updatePositions(
        scenario,
        [&](Process::ProcessModel& p, const TimeVal& t) {
          p.setParentDuration(m_mode, t);
        },
        m_savedElementsProperties);
  }

  const Path<Scenario::ProcessModel>& path() const override { return m_path; }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_savedElementsProperties << m_path << m_eventId << m_initialDate
      << (int)m_mode;
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    int mode;
    s >> m_savedElementsProperties >> m_path >> m_eventId >> m_initialDate
        >> mode;

    m_mode = static_cast<ExpandMode>(mode);
  }

private:
  ElementsProperties m_savedElementsProperties;
  Path<Scenario::ProcessModel> m_path;

  ExpandMode m_mode{ExpandMode::Scale};
  LockMode m_lock{LockMode::Free};

  Id<EventModel> m_eventId;
  /**
   * @brief m_initialDate
   * the delta will be calculated from the initial date
   */
  TimeVal
      m_initialDate; // used to compute the deltaTime and respect undo behavior
};
}
}

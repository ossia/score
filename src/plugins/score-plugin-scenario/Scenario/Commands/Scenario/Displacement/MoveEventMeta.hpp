#pragma once

#include "MoveEventFactoryInterface.hpp"

#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Scenario/Displacement/SerializableMoveEvent.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

namespace Scenario
{
class EventModel;
class ProcessModel;
class StateModel;
namespace Command
{

class SCORE_PLUGIN_SCENARIO_EXPORT MoveEventMeta final
    : public SerializableMoveEvent
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      MoveEventMeta,
      "Move an event")

public:
  MoveEventMeta(
      const Scenario::ProcessModel& scenarioPath,
      Id<EventModel> eventId,
      TimeVal newDate,
      double y,
      ExpandMode mode,
      LockMode lock);

  MoveEventMeta(
      const Scenario::ProcessModel& scenarioPath,
      Id<EventModel> eventId,
      TimeVal newDate,
      double y,
      ExpandMode mode,
      LockMode lock,
      Id<StateModel>);
  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  const Path<Scenario::ProcessModel>& path() const override;

  void update(
      Scenario::ProcessModel& scenar,
      const Id<EventModel>& eventId,
      const TimeVal& newDate,
      double y,
      ExpandMode mode,
      LockMode lock) override;
  void update(
      Scenario::ProcessModel& scenar,
      const Id<EventModel>& eventId,
      const TimeVal& newDate,
      double y,
      ExpandMode mode,
      LockMode lock,
      const Id<StateModel>& st);

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  void updateY(Scenario::ProcessModel&, double y) const;
  // TODO : make a UI to change that
  Path<Scenario::ProcessModel> m_scenario;
  Id<EventModel> m_eventId;
  optional<Id<StateModel>> m_stateId;
  LockMode m_lock{};
  double m_oldY{};
  double m_newY{};

  std::unique_ptr<SerializableMoveEvent> m_moveEventImplementation{};
};

class SCORE_PLUGIN_SCENARIO_EXPORT MoveTopEventMeta final
    : public SerializableMoveEvent
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      MoveTopEventMeta,
      "Move an event")

public:
  MoveTopEventMeta(
      const Scenario::ProcessModel& scenarioPath,
      Id<EventModel> eventId,
      TimeVal newDate,
      double y,
      ExpandMode mode,
      LockMode lock);
  MoveTopEventMeta(
      const Scenario::ProcessModel& scenarioPath,
      Id<EventModel> eventId,
      TimeVal newDate,
      double y,
      ExpandMode mode,
      LockMode lock,
      Id<StateModel>)
      : MoveTopEventMeta{scenarioPath, eventId, newDate, y, mode, lock}
  {
  }

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  const Path<Scenario::ProcessModel>& path() const override;

  void update(
      Scenario::ProcessModel&,
      const Id<EventModel>& eventId,
      const TimeVal& newDate,
      double y,
      ExpandMode mode,
      LockMode) override;
  void update(
      Scenario::ProcessModel& s,
      const Id<EventModel>& eventId,
      const TimeVal& newDate,
      double y,
      ExpandMode mode,
      LockMode,
      const Id<StateModel>&)
  {
    update(s, eventId, newDate, y, mode, LockMode::Free);
  }

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  // TODO : make a UI to change that
  Path<Scenario::ProcessModel> m_scenario;
  Id<EventModel> m_eventId;
  std::unique_ptr<SerializableMoveEvent> m_moveEventImplementation{};
};

class MoveIntervalMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      MoveIntervalMacro,
      "Move an interval")
};

class MoveStateMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      MoveStateMacro,
      "Move a state")
};
}
}

#pragma once

#include <Scenario/Commands/Scenario/Displacement/SerializableMoveEvent.hpp>

#include "MoveEventFactoryInterface.hpp"
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/model/path/Path.hpp>

#include <iscore/model/Identifier.hpp>
struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class EventModel;
class ProcessModel;
class StateModel;
namespace Command
{

class ISCORE_PLUGIN_SCENARIO_EXPORT MoveEventMeta final
    : public SerializableMoveEvent
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), MoveEventMeta, "Move an event")

public:
  MoveEventMeta(
      const Scenario::ProcessModel& scenarioPath,
      Id<EventModel>
          eventId,
      TimeVal newDate,
      double y,
      ExpandMode mode);

  MoveEventMeta(
      const Scenario::ProcessModel& scenarioPath,
      Id<EventModel>
          eventId,
      TimeVal newDate,
      double y,
      ExpandMode mode,
      Id<StateModel>);
  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

  const Path<Scenario::ProcessModel>& path() const override;

  void update(
      Scenario::ProcessModel& scenar,
      const Id<EventModel>& eventId,
      const TimeVal& newDate,
      double y, ExpandMode mode) override
  {
    m_moveEventImplementation->update(scenar, eventId, newDate, y, mode);
    m_newY = y;
    updateY(scenar, m_newY);
  }
  void update(
      Scenario::ProcessModel& scenar,
      const Id<EventModel>& eventId,
      const TimeVal& newDate,
      double y, ExpandMode mode,
      Id<StateModel> st)
  {
    m_moveEventImplementation->update(scenar, eventId, newDate, y, mode);
    m_newY = y;
    updateY(scenar, m_newY);
  }

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  void updateY(Scenario::ProcessModel&, double y) const;
  // TODO : make a UI to change that
  Path<Scenario::ProcessModel> m_scenario;
  Id<EventModel> m_eventId;
  optional<Id<StateModel>> m_stateId;
  double m_oldY{};
  double m_newY{};

  std::unique_ptr<SerializableMoveEvent> m_moveEventImplementation{};
};



class ISCORE_PLUGIN_SCENARIO_EXPORT MoveTopEventMeta final
    : public SerializableMoveEvent
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), MoveTopEventMeta, "Move an event")

public:
  MoveTopEventMeta(
      const Scenario::ProcessModel& scenarioPath,
      Id<EventModel>
          eventId,
      TimeVal newDate,
      double y,
      ExpandMode mode);
  MoveTopEventMeta(
          const Scenario::ProcessModel& scenarioPath,
          Id<EventModel>
          eventId,
          TimeVal newDate,
          double y,
          ExpandMode mode, Id<StateModel> )
      : MoveTopEventMeta{scenarioPath, eventId, newDate, y ,mode}
  { }

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

  const Path<Scenario::ProcessModel>& path() const override;

  void update(
      Scenario::ProcessModel&, const Id<EventModel>& eventId, const TimeVal& newDate,
      double y, ExpandMode mode) override;
  void update(
          Scenario::ProcessModel& s, const Id<EventModel>& eventId, const TimeVal& newDate,
          double y, ExpandMode mode, const Id<StateModel>&)
  {
      update(s, eventId, newDate, y, mode);
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
}
}

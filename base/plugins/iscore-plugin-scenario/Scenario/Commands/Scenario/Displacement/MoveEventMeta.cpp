#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>

#include <QByteArray>
#include <algorithm>

#include "MoveEventMeta.hpp"
#include <Scenario/Commands/Scenario/Displacement/SerializableMoveEvent.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
class EventModel;
namespace Command
{
MoveEventMeta::MoveEventMeta(
    const Scenario::ProcessModel& scenar,
    Id<EventModel> eventId,
    TimeVal newDate,
    double y,
    ExpandMode mode)
    : SerializableMoveEvent{}
    , m_scenario{scenar}
    , m_eventId{std::move(eventId)}
    , m_newY{y}
    , m_moveEventImplementation(
          iscore::AppContext().interfaces<MoveEventList>()
              .get(iscore::AppContext(), MoveEventFactoryInterface::Strategy::MOVE)
              .make(
                  scenar, m_eventId, std::move(newDate),
                  mode))
{
  auto& ev = scenar.event(m_eventId);
  auto states = ev.states();
  if (states.size() == 1)
  {
    auto& st = scenar.states.at(states.front());
    m_oldY = st.heightPercentage();
  }
}


MoveEventMeta::MoveEventMeta(
    const Scenario::ProcessModel& scenar,
    Id<EventModel> eventId,
    TimeVal newDate,
    double y,
    ExpandMode mode,
        Id<StateModel> sid)
    : SerializableMoveEvent{}
    , m_scenario{scenar}
    , m_eventId{std::move(eventId)}
    , m_stateId{std::move(sid)}
    , m_newY{y}
    , m_moveEventImplementation(
          iscore::AppContext().interfaces<MoveEventList>()
              .get(iscore::AppContext(), MoveEventFactoryInterface::Strategy::MOVE)
              .make(
                  scenar, m_eventId, std::move(newDate),
                  mode))
{
  m_oldY = scenar.state(*m_stateId).heightPercentage();
}


void MoveEventMeta::undo(const iscore::DocumentContext& ctx) const
{
  m_moveEventImplementation->undo(ctx);
  updateY(m_scenario.find(ctx), m_oldY);
}

void MoveEventMeta::redo(const iscore::DocumentContext& ctx) const
{
  m_moveEventImplementation->redo(ctx);
  updateY(m_scenario.find(ctx), m_newY);
}

const Path<Scenario::ProcessModel>& MoveEventMeta::path() const
{
  return m_moveEventImplementation->path();
}

void MoveEventMeta::serializeImpl(DataStreamInput& s) const
{
  s << m_scenario << m_eventId << m_stateId << m_oldY << m_newY
    << m_moveEventImplementation->serialize();
}

void MoveEventMeta::deserializeImpl(DataStreamOutput& s)
{
  QByteArray cmdData;
  s >> m_scenario >> m_eventId >> m_stateId >> m_oldY >> m_newY >> cmdData;

  m_moveEventImplementation
      = iscore::AppContext().interfaces<MoveEventList>()
            .get(iscore::AppContext(), MoveEventFactoryInterface::Strategy::MOVE)
            .make();

  m_moveEventImplementation->deserialize(cmdData);
}

void MoveEventMeta::updateY(Scenario::ProcessModel& scenar, double y) const
{
    auto& ev = scenar.event(m_eventId);
    auto states = ev.states();
    Scenario::StateModel* stp{};
    if(m_stateId)
    {
        stp = &scenar.states.at(*m_stateId);
    }
    else if (states.size() == 1)
    {
        stp = &scenar.states.at(states.front());
    }

    if(stp)
    {
        auto& st = *stp;
        if (!st.previousConstraint() && !st.nextConstraint())
        {
          st.setHeightPercentage(y);
        }
        if (st.previousConstraint())
          updateConstraintVerticalPos(y, *st.previousConstraint(), scenar);
        if (st.nextConstraint())
          updateConstraintVerticalPos(y, *st.nextConstraint(), scenar);
    }
}


MoveTopEventMeta::MoveTopEventMeta(
    const Scenario::ProcessModel& scenarioPath,
    Id<EventModel> eventId,
    TimeVal newDate,
    double y,
    ExpandMode mode)
    : SerializableMoveEvent{}
    , m_scenario{scenarioPath}
    , m_eventId{std::move(eventId)}
    , m_moveEventImplementation(
          iscore::AppContext().interfaces<MoveEventList>()
              .get(iscore::AppContext(), MoveEventFactoryInterface::Strategy::MOVE)
              .make(
                  scenarioPath, m_eventId, std::move(newDate),
                  mode))
{
}

void MoveTopEventMeta::undo(const iscore::DocumentContext& ctx) const
{
  m_moveEventImplementation->undo(ctx);
}

void MoveTopEventMeta::redo(const iscore::DocumentContext& ctx) const
{
  m_moveEventImplementation->redo(ctx);
}

const Path<Scenario::ProcessModel>& MoveTopEventMeta::path() const
{
  return m_moveEventImplementation->path();
}

void MoveTopEventMeta::serializeImpl(DataStreamInput& s) const
{
  s << m_scenario << m_eventId << m_moveEventImplementation->serialize();
}

void MoveTopEventMeta::deserializeImpl(DataStreamOutput& s)
{
  QByteArray cmdData;
  s >> m_scenario >> m_eventId >> cmdData;

  m_moveEventImplementation
      = iscore::AppContext().interfaces<MoveEventList>()
            .get(iscore::AppContext(), MoveEventFactoryInterface::Strategy::MOVE)
            .make();

  m_moveEventImplementation->deserialize(cmdData);
}

void MoveTopEventMeta::update(Scenario::ProcessModel& scenar, const Id<EventModel>& eventId, const TimeVal& newDate, double y, ExpandMode mode)
{
  m_moveEventImplementation->update(scenar, eventId, newDate, y, mode);
}
}
}

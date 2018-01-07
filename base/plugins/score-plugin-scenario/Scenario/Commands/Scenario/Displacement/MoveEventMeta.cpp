// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>

#include <QByteArray>
#include <algorithm>

#include "MoveEventMeta.hpp"
#include <Scenario/Commands/Scenario/Displacement/SerializableMoveEvent.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/Identifier.hpp>

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
    ExpandMode mode,
    LockMode lm)
    : SerializableMoveEvent{}
    , m_scenario{scenar}
    , m_eventId{std::move(eventId)}
    , m_lock{lm}
    , m_newY{y}
    , m_moveEventImplementation(
          score::AppContext().interfaces<MoveEventList>()
              .get(score::AppContext(), MoveEventFactoryInterface::Strategy::MOVE)
              .make(
                  scenar, m_eventId, std::move(newDate),
                  mode, lm))
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
    LockMode lm,
        Id<StateModel> sid)
    : SerializableMoveEvent{}
    , m_scenario{scenar}
    , m_eventId{std::move(eventId)}
    , m_stateId{std::move(sid)}
    , m_lock{lm}
    , m_newY{y}
    , m_moveEventImplementation(
          score::AppContext().interfaces<MoveEventList>()
              .get(score::AppContext(), MoveEventFactoryInterface::Strategy::MOVE)
              .make(
                  scenar, m_eventId, std::move(newDate),
                  mode, lm))
{
  m_oldY = scenar.state(*m_stateId).heightPercentage();
}


void MoveEventMeta::undo(const score::DocumentContext& ctx) const
{
  m_moveEventImplementation->undo(ctx);
  updateY(m_scenario.find(ctx), m_oldY);
}

void MoveEventMeta::redo(const score::DocumentContext& ctx) const
{
  m_moveEventImplementation->redo(ctx);
  updateY(m_scenario.find(ctx), m_newY);
}

const Path<Scenario::ProcessModel>& MoveEventMeta::path() const
{
  return m_moveEventImplementation->path();
}

void MoveEventMeta::update(
    ProcessModel& scenar,
    const Id<EventModel>& eventId,
    const TimeVal& newDate,
    double y,
    ExpandMode mode,
    LockMode lock)
{
  if(lock == m_lock)
  {
    m_moveEventImplementation->update(scenar, eventId, newDate, y, mode, lock);
  }
  else
  {
    static const auto& appctx = score::AppContext();
    static const auto& mevlist = appctx.interfaces<MoveEventList>();
    m_moveEventImplementation = mevlist
            .get(appctx, MoveEventFactoryInterface::Strategy::MOVE)
            .make(scenar, eventId, newDate, mode, lock);
    m_lock = lock;
  }
  m_newY = y;
  updateY(scenar, m_newY);
}

void MoveEventMeta::update(
    ProcessModel& scenar,
    const Id<EventModel>& eventId,
    const TimeVal& newDate,
    double y,
    ExpandMode mode,
    LockMode lock,
    const Id<StateModel>& st)
{
  if(lock == m_lock)
  {
    m_moveEventImplementation->update(scenar, eventId, newDate, y, mode, lock);
  }
  else
  {
    static const auto& appctx = score::AppContext();
    static const auto& mevlist = appctx.interfaces<MoveEventList>();
    m_moveEventImplementation = mevlist
            .get(appctx, MoveEventFactoryInterface::Strategy::MOVE)
            .make(scenar, eventId, newDate, mode, lock);
    m_lock = lock;
  }
  m_newY = y;
  updateY(scenar, m_newY);
}

void MoveEventMeta::serializeImpl(DataStreamInput& s) const
{
  s << m_scenario << m_eventId << m_stateId << (int&)m_lock <<  m_oldY << m_newY
    << m_moveEventImplementation->serialize();
}

void MoveEventMeta::deserializeImpl(DataStreamOutput& s)
{
  QByteArray cmdData;
  s >> m_scenario >> m_eventId >> m_stateId >> (int&)m_lock >> m_oldY >> m_newY >> cmdData;

  m_moveEventImplementation
      = score::AppContext().interfaces<MoveEventList>()
            .get(score::AppContext(), MoveEventFactoryInterface::Strategy::MOVE)
            .make(m_lock);

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
        if (!st.previousInterval() && !st.nextInterval())
        {
          st.setHeightPercentage(y);
        }
        if (st.previousInterval())
          updateIntervalVerticalPos(y, *st.previousInterval(), scenar);
        if (st.nextInterval())
          updateIntervalVerticalPos(y, *st.nextInterval(), scenar);
    }
}


MoveTopEventMeta::MoveTopEventMeta(
    const Scenario::ProcessModel& scenarioPath,
    Id<EventModel> eventId,
    TimeVal newDate,
    double y,
    ExpandMode mode, LockMode)
    : SerializableMoveEvent{}
    , m_scenario{scenarioPath}
    , m_eventId{std::move(eventId)}
    , m_moveEventImplementation(
          score::AppContext().interfaces<MoveEventList>()
              .get(score::AppContext(), MoveEventFactoryInterface::Strategy::MOVE)
              .make(
                  scenarioPath, m_eventId, std::move(newDate),
                  mode, LockMode::Free))
{
}

void MoveTopEventMeta::undo(const score::DocumentContext& ctx) const
{
  m_moveEventImplementation->undo(ctx);
}

void MoveTopEventMeta::redo(const score::DocumentContext& ctx) const
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
      = score::AppContext().interfaces<MoveEventList>()
            .get(score::AppContext(), MoveEventFactoryInterface::Strategy::MOVE)
            .make(LockMode::Free);

  m_moveEventImplementation->deserialize(cmdData);
}

void MoveTopEventMeta::update(Scenario::ProcessModel& scenar,
                              const Id<EventModel>& eventId,
                              const TimeVal& newDate,
                              double y,
                              ExpandMode mode,
                              LockMode lm)
{
  m_moveEventImplementation->update(scenar, eventId, newDate, y, mode, LockMode::Free);
}
}
}

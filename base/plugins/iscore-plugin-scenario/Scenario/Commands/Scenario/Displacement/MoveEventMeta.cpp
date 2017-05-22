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
    Path<Scenario::ProcessModel>&& scenarioPath,
    Id<EventModel>
        eventId,
    TimeVal newDate,
    double y,
    ExpandMode mode)
    : SerializableMoveEvent{}
    , m_scenario{scenarioPath}
    , m_eventId{std::move(eventId)}
    , m_newY{y}
    , m_moveEventImplementation(
          context.interfaces<MoveEventList>()
              .get(context, MoveEventFactoryInterface::Strategy::MOVE)
              .make(
                  std::move(scenarioPath), m_eventId, std::move(newDate),
                  mode))
{
  auto& scenar = m_scenario.find();
  auto& ev = scenar.event(m_eventId);
  auto states = ev.states();
  if (states.size() == 1)
  {
    auto& st = scenar.states.at(states.front());
    m_oldY = st.heightPercentage();
  }
}

void MoveEventMeta::undo() const
{
  m_moveEventImplementation->undo();
  updateY(m_oldY);
}

void MoveEventMeta::redo() const
{
  m_moveEventImplementation->redo();
  updateY(m_newY);
}

const Path<Scenario::ProcessModel>& MoveEventMeta::path() const
{
  return m_moveEventImplementation->path();
}

void MoveEventMeta::serializeImpl(DataStreamInput& s) const
{
  s << m_scenario << m_eventId << m_oldY << m_newY
    << m_moveEventImplementation->serialize();
}

void MoveEventMeta::deserializeImpl(DataStreamOutput& s)
{
  QByteArray cmdData;
  s >> m_scenario >> m_eventId >> m_oldY >> m_newY >> cmdData;

  m_moveEventImplementation
      = context.interfaces<MoveEventList>()
            .get(context, MoveEventFactoryInterface::Strategy::MOVE)
            .make();

  m_moveEventImplementation->deserialize(cmdData);
}

void MoveEventMeta::updateY(double y) const
{
    if (!m_scenario.valid())
      return;

    auto& scenar = m_scenario.find();
    auto& ev = scenar.event(m_eventId);
    auto states = ev.states();
    if (states.size() == 1)
    {
      auto& st = scenar.states.at(states.front());
      if (!st.previousConstraint() && !st.nextConstraint())
        st.setHeightPercentage(y);
      if (st.previousConstraint())
        updateConstraintVerticalPos(y, *st.previousConstraint(), scenar);
      if (st.nextConstraint())
        updateConstraintVerticalPos(y, *st.nextConstraint(), scenar);
    }
}

void MoveEventMeta::update(
    const Id<EventModel>& eventId, const TimeVal& newDate, double y,
    ExpandMode mode)
{
  m_moveEventImplementation->update(eventId, newDate, y, mode);
  m_newY = y;
  updateY(m_newY);
}





MoveTopEventMeta::MoveTopEventMeta(
    Path<Scenario::ProcessModel>&& scenarioPath,
    Id<EventModel>
        eventId,
    TimeVal newDate,
    double y,
    ExpandMode mode)
    : SerializableMoveEvent{}
    , m_scenario{scenarioPath}
    , m_eventId{std::move(eventId)}
    , m_moveEventImplementation(
          context.interfaces<MoveEventList>()
              .get(context, MoveEventFactoryInterface::Strategy::MOVE)
              .make(
                  std::move(scenarioPath), m_eventId, std::move(newDate),
                  mode))
{
}

void MoveTopEventMeta::undo() const
{
  m_moveEventImplementation->undo();
}

void MoveTopEventMeta::redo() const
{
  m_moveEventImplementation->redo();
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
      = context.interfaces<MoveEventList>()
            .get(context, MoveEventFactoryInterface::Strategy::MOVE)
            .make();

  m_moveEventImplementation->deserialize(cmdData);
}

void MoveTopEventMeta::update(
    const Id<EventModel>& eventId, const TimeVal& newDate, double y,
    ExpandMode mode)
{
  m_moveEventImplementation->update(eventId, newDate, y, mode);
}

void MoveTopEventMeta::update(unused_t, const Id<EventModel>& eventId, const TimeVal& newDate, double y, ExpandMode mode)
{
  m_moveEventImplementation->update(eventId, newDate, y, mode);
}
}
}

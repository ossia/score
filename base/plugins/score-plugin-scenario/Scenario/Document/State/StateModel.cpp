// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/State/ProcessStateDataInterface.hpp>
#include <QAbstractItemModel>
#include <QObject>
#include <algorithm>
#include <score/document/DocumentInterface.hpp>

#include "ItemModel/MessageItemModelAlgorithms.hpp"
#include "StateModel.hpp"
#include <Process/Process.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <State/Message.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>

namespace Scenario
{
StateModel::StateModel(
    const Id<StateModel>& id,
    const Id<EventModel>& eventId,
    double yPos,
    const score::CommandStackFacade& stack,
    QObject* parent)
    : Entity{id, Metadata<ObjectKey_k, StateModel>::get(), parent}
    , m_stack{stack}
    , m_eventId{eventId}
    , m_heightPercentage{yPos}
    , m_messageItemModel{new MessageItemModel{stack, *this, this}}
{
  metadata().setInstanceName(*this);
  init();
}

void StateModel::init()
{
  con(m_messageItemModel, &QAbstractItemModel::modelReset, this,
      &StateModel::statesUpdated_slt);
  con(m_messageItemModel, &QAbstractItemModel::dataChanged, this,
      &StateModel::statesUpdated_slt);
  con(m_messageItemModel, &QAbstractItemModel::rowsInserted, this,
      &StateModel::statesUpdated_slt);
  con(m_messageItemModel, &QAbstractItemModel::rowsMoved, this,
      &StateModel::statesUpdated_slt);
  con(m_messageItemModel, &QAbstractItemModel::rowsRemoved, this,
      &StateModel::statesUpdated_slt);
}

double StateModel::heightPercentage() const
{
  return m_heightPercentage;
}

void StateModel::setHeightPercentage(double y)
{
  if (m_heightPercentage == y)
    return;
  m_heightPercentage = y;
  emit heightPercentageChanged();
}

void StateModel::statesUpdated_slt()
{
  emit sig_statesUpdated();
}

const Id<EventModel>& StateModel::eventId() const
{
  return m_eventId;
}

void StateModel::setEventId(const Id<EventModel>& id)
{
  m_eventId = id;
}

const OptionalId<IntervalModel>& StateModel::previousInterval() const
{
  return m_previousInterval;
}

const OptionalId<IntervalModel>& StateModel::nextInterval() const
{
  return m_nextInterval;
}

void StateModel::setNextInterval(const OptionalId<IntervalModel>& id)
{
  m_nextInterval = id;
}

void StateModel::setPreviousInterval(const OptionalId<IntervalModel>& id)
{
  m_previousInterval = id;
}

MessageItemModel& StateModel::messages() const
{
  return *m_messageItemModel;
}

void StateModel::setStatus(ExecutionStatus status)
{
  if (m_status.get() == status)
    return;

  m_status.set(status);
  emit statusChanged(status);
}

ProcessStateWrapper::~ProcessStateWrapper()
{

}

}

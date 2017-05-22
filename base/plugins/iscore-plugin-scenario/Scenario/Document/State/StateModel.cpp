#include <Process/State/ProcessStateDataInterface.hpp>
#include <QAbstractItemModel>
#include <QObject>
#include <algorithm>
#include <iscore/document/DocumentInterface.hpp>

#include "ItemModel/MessageItemModelAlgorithms.hpp"
#include "StateModel.hpp"
#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <State/Message.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
StateModel::StateModel(
    const Id<StateModel>& id,
    const Id<EventModel>& eventId,
    double yPos,
    const iscore::CommandStackFacade& stack,
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

StateModel::StateModel(
    const StateModel& source,
    const Id<StateModel>& id,
    const iscore::CommandStackFacade& stack,
    QObject* parent)
    : Entity{source, id, Metadata<ObjectKey_k, StateModel>::get(), parent}
    , m_stack{stack}
    , m_eventId{source.eventId()}
    , m_previousConstraint{source.previousConstraint()}
    , m_nextConstraint{source.nextConstraint()}
    , m_heightPercentage{source.heightPercentage()}
    , m_messageItemModel{new MessageItemModel{m_stack, *this, this}}
{
  // FIXME Source has to be in the same document else it will crash.
  // FIXME prune the messages from the prev / next processes data and rebuild
  // it.
  messages() = source.messages();

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

const OptionalId<ConstraintModel>& StateModel::previousConstraint() const
{
  return m_previousConstraint;
}

const OptionalId<ConstraintModel>& StateModel::nextConstraint() const
{
  return m_nextConstraint;
}

void StateModel::setNextConstraint(const OptionalId<ConstraintModel>& id)
{
  m_nextConstraint = id;
}

void StateModel::setPreviousConstraint(const OptionalId<ConstraintModel>& id)
{
  m_previousConstraint = id;
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
}

// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StateModel.hpp"

#include "ItemModel/MessageItemModelAlgorithms.hpp"

#include <State/Message.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>

#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>

#include <LocalTree/ScriptableProcessComponent.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/Bind.hpp>

#include <QAbstractItemModel>
#include <QObject>
#include <QTimer>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Scenario::StateModel)
namespace Scenario
{
StateModel::StateModel(
    const Id<StateModel>& id, const Id<EventModel>& eventId, double yPos,
    const score::DocumentContext& ctx, QObject* parent)
    : Entity{id, Metadata<ObjectKey_k, StateModel>::get(), parent}
    , m_context{ctx}
    , m_eventId{eventId}
    , m_heightPercentage{yPos}
    , m_messageItemModel{new MessageItemModel{*this, this}}
{
  metadata().setInstanceName(*this);
  init();
}

StateModel::~StateModel()
{
  delete m_messageItemModel;
}

void StateModel::init()
{
  con(*m_messageItemModel, &QAbstractItemModel::modelReset, this,
      &StateModel::statesUpdated_slt);
  con(*m_messageItemModel, &QAbstractItemModel::dataChanged, this,
      &StateModel::statesUpdated_slt);
  con(*m_messageItemModel, &QAbstractItemModel::rowsInserted, this,
      &StateModel::statesUpdated_slt);
  con(*m_messageItemModel, &QAbstractItemModel::rowsMoved, this,
      &StateModel::statesUpdated_slt);
  con(*m_messageItemModel, &QAbstractItemModel::rowsRemoved, this,
      &StateModel::statesUpdated_slt);

  // Ports only resolve once the document is loaded; posted to run before a
  // crash backup's commands are replayed.
  if(!m_legacyControls.empty())
    QMetaObject::invokeMethod(
        this, &StateModel::migrateLegacyControls, Qt::QueuedConnection);
}

void StateModel::migrateLegacyControls()
{
  State::MessageList list;
  for(auto& [path, value] : m_legacyControls)
  {
    auto port = qobject_cast<Process::ControlInlet*>(path.try_find(m_context));
    if(!port)
      continue;
    port->setScriptable(true);
    if(auto addr = LocalTree::scriptableAddress(*port); addr.isSet())
      list.push_back({State::AddressAccessor{addr}, value});
  }
  m_legacyControls.clear();
  if(list.empty())
    return;

  auto node = messages().rootNode();
  updateTreeWithMessageList(node, std::move(list));
  messages() = std::move(node);
}

double StateModel::heightPercentage() const
{
  return m_heightPercentage;
}

void StateModel::setHeightPercentage(double y)
{
  if(m_heightPercentage == y)
    return;
  m_heightPercentage = y;
  heightPercentageChanged();
}

void StateModel::statesUpdated_slt()
{
  sig_statesUpdated();
}

const Id<EventModel>& StateModel::eventId() const
{
  return m_eventId;
}

void StateModel::setEventId(const Id<EventModel>& id)
{
  if(id != m_eventId)
  {
    auto old = m_eventId;
    m_eventId = id;
    eventChanged(old, id);
  }
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
  if(m_status.get() == status)
    return;

  m_status.set(status);
  statusChanged(status);
}

ProcessStateWrapper::~ProcessStateWrapper() { }
}

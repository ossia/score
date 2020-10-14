#include "ObjectItemModel.hpp"

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Interval/CreateProcessInNewSlot.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Commands/Interval/SetProcessPosition.hpp>
#include <Scenario/Commands/State/AddStateProcess.hpp>
#include <Scenario/Commands/State/RemoveStateProcess.hpp>
#include <Scenario/DialogWidget/AddProcessDialog.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QAction>
#include <QDebug>
#include <QHeaderView>
#include <QMenu>
#include <QToolButton>

#include <Effect/EffectFactory.hpp>

// SearchWidget
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>
#include <Explorer/Panel/DeviceExplorerPanelDelegate.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <State/MessageListSerialization.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/widgets/ArrowButton.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/presenter/DocumentManager.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::ObjectItemModel)
namespace Scenario
{

ObjectItemModel::ObjectItemModel(const score::DocumentContext& ctx, QObject* parent)
    : QAbstractItemModel{parent}, m_ctx{ctx}
{
}

void ObjectItemModel::setSelected(QList<const IdentifiedObjectAbstract*> objs)
{
  QList<const QObject*> root{};
  for (const QObject* sel : objs)
  {
    if (auto cst = qobject_cast<const Scenario::IntervalModel*>(sel))
    {
      root.push_back(cst);
    }
    else if (auto ts = qobject_cast<const Scenario::TimeSyncModel*>(sel))
    {
      root.push_back(ts);
    }
  }

  for (const QObject* sel : objs)
  {
    if (auto ev = qobject_cast<const Scenario::EventModel*>(sel))
    {
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*ev);
      auto& parent_ts = Scenario::parentTimeSync(*ev, scenar);
      if (parent_ts.events().size() > 1 || parent_ts.active())
        root.push_back(&parent_ts);
      else if (!root.contains(&parent_ts))
        root.push_back(ev);
    }
    else if (auto st = qobject_cast<const Scenario::StateModel*>(sel))
    {
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*st);
      auto& parent_ev = Scenario::parentEvent(*st, scenar);
      auto& parent_ts = Scenario::parentTimeSync(parent_ev, scenar);
      if (parent_ev.states().size() > 1 || parent_ev.condition() != State::Expression{})
      {
        if (parent_ts.events().size() > 1 || parent_ts.active())
          root.push_back(&parent_ts);
        else if (!root.contains(&parent_ev) && !root.contains(&parent_ts))
          root.push_back(&parent_ev);
      }
      else if (!root.contains(&parent_ev) && !root.contains(&parent_ts))
        root.push_back(st);
    }
    else if (auto proc = qobject_cast<const Process::ProcessModel*>(sel))
    {
      if (auto state = qobject_cast<Scenario::StateModel*>(proc->parent()))
      {
        Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*state);
        root.push_back(&Scenario::parentTimeSync(*state, scenar));
      }
      else if (auto itv = qobject_cast<Scenario::IntervalModel*>(proc->parent()))
      {
        root.push_back(itv);
      }
    }
  }

  root = root.toSet().toList();
  if (root != m_root)
  {
    cleanConnections();

    beginResetModel();
    m_root = root;
    endResetModel();

    setupConnections();
  }
}

void ObjectItemModel::setupConnections()
{
  if (m_root.empty())
    return;
  m_aliveMap.clear();

  for (auto obj : m_root)
  {
    m_aliveMap.insert(obj, obj);
    if (auto cst = qobject_cast<const Scenario::IntervalModel*>(obj))
    {
      cst->processes.added.connect<&ObjectItemModel::recompute<const Process::ProcessModel&>>(
          *this);
      cst->processes.removed.connect<&ObjectItemModel::recompute<const Process::ProcessModel&>>(
          *this);

      for (auto& proc : cst->processes)
        m_aliveMap.insert(&proc, &proc);
    }
    else if (auto tn = qobject_cast<const Scenario::TimeSyncModel*>(obj))
    {
      auto& scenar = Scenario::parentScenario(*tn);
      m_itemCon.push_back(connect(tn, &TimeSyncModel::newEvent, this, [=] { recompute(); }));
      m_itemCon.push_back(connect(tn, &TimeSyncModel::eventRemoved, this, [=] { recompute(); }));

      for (const auto& ev : tn->events())
      {
        if (auto* eptr = scenar.findEvent(ev))
        {
          m_aliveMap.insert(eptr, eptr);
          m_itemCon.push_back(
              connect(eptr, &EventModel::statesChanged, this, [=] { recompute(); }));
          for (const auto& st : eptr->states())
          {
            if (auto* sptr = scenar.findState(st))
            {
              m_aliveMap.insert(sptr, sptr);
              sptr->stateProcesses.added
                  .connect<&ObjectItemModel::recompute<const Process::ProcessModel&>>(*this);
              sptr->stateProcesses.removed
                  .connect<&ObjectItemModel::recompute<const Process::ProcessModel&>>(*this);

              for (const auto& sp : sptr->stateProcesses)
                m_aliveMap.insert(&sp, &sp);
            }
          }
        }
      }
    }
    else if (auto ev = qobject_cast<const Scenario::EventModel*>(obj))
    {
      auto& scenar = Scenario::parentScenario(*ev);
      auto& e = *ev;
      m_aliveMap.insert(&e, &e);
      m_itemCon.push_back(con(e, &EventModel::statesChanged, this, [=] { recompute(); }));

      for (const auto& st : e.states())
      {
        if (auto* sptr = scenar.findState(st))
        {
          m_aliveMap.insert(sptr, sptr);
          sptr->stateProcesses.added
              .connect<&ObjectItemModel::recompute<const Process::ProcessModel&>>(*this);
          sptr->stateProcesses.removed
              .connect<&ObjectItemModel::recompute<const Process::ProcessModel&>>(*this);

          for (const auto& sp : sptr->stateProcesses)
            m_aliveMap.insert(&sp, &sp);
        }
      }
    }
    else if (auto st = qobject_cast<const Scenario::StateModel*>(obj))
    {
      auto& s = *st;
      m_aliveMap.insert(&s, &s);
      s.stateProcesses.added.connect<&ObjectItemModel::recompute<const Process::ProcessModel&>>(
          *this);
      s.stateProcesses.removed.connect<&ObjectItemModel::recompute<const Process::ProcessModel&>>(
          *this);

      for (const auto& sp : s.stateProcesses)
        m_aliveMap.insert(&sp, &sp);
    }

    m_itemCon.push_back(connect(obj, &QObject::destroyed, this, [=] {
      m_root.removeOne(obj);
      cleanConnections();

      beginResetModel();
      endResetModel();

      setupConnections();
    }));
  }
}

void ObjectItemModel::cleanConnections()
{
  this->removeAll();
  disconnect(m_con);
  for (auto& con : m_itemCon)
    QObject::disconnect(con);
  m_itemCon.clear();
}

QModelIndex ObjectItemModel::index(int row, int column, const QModelIndex& parent) const
{
  auto sel = (QObject*)parent.internalPointer();
  if (isAlive(sel))
  {
    if (auto cst = qobject_cast<Scenario::IntervalModel*>(sel))
    {
      if (row < (int)cst->processes.size())
      {
        auto it = cst->processes.begin();
        std::advance(it, row);
        return createIndex(row, column, &*(it));
      }
      else
      {
        qWarning("Interval: wrong size! ");
      }
    }
    else if (auto ev = qobject_cast<Scenario::EventModel*>(sel))
    {
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*ev);
      if (row < (int)ev->states().size())
      {
        auto it = ev->states().begin();
        std::advance(it, row);

        if (auto st = scenar.findState(*it))
          return createIndex(row, column, st);
      }
      else
      {
        qWarning() << "Event: wrong size! " << row;
      }
    }
    else if (auto tn = qobject_cast<Scenario::TimeSyncModel*>(sel))
    {
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*tn);
      if (row < (int)tn->events().size())
      {
        auto it = tn->events().begin();
        std::advance(it, row);

        if (auto ev = scenar.findEvent(*it))
          return createIndex(row, column, ev);
      }
      else
      {
        qWarning("TN: wrong size! ");
      }
    }
    else if (auto st = qobject_cast<Scenario::StateModel*>(sel))
    {
      if (row < (int)st->stateProcesses.size())
      {
        auto it = st->stateProcesses.begin();
        std::advance(it, row);
        auto& proc = *it;
        return createIndex(row, column, &proc);
      }
      else
      {
        qWarning() << "State: wrong size! " << row;
      }
    }
    else
    {
      return QModelIndex{};
    }
  }
  else if (!m_root.empty() && row >= 0)
  {
    return createIndex(row, column, (void*)m_root[row]);
  }
  else
  {
    return QModelIndex{};
  }
  return QModelIndex{};
}

bool ObjectItemModel::isAlive(QObject* obj) const
{
  if (!obj)
    return false;

  auto it = m_aliveMap.find(obj);
  if (it != m_aliveMap.end())
  {
    if (it->isNull())
    {
      m_aliveMap.erase(it);
      return false;
    }
    return true;
  }
  return false;
}

QModelIndex ObjectItemModel::parent(const QModelIndex& child) const
{
  auto sel = (QObject*)child.internalPointer();
  if (!isAlive(sel))
    return QModelIndex{};

  if (qobject_cast<Scenario::IntervalModel*>(sel))
  {
    return QModelIndex{};
  }
  else if (auto ev = qobject_cast<Scenario::EventModel*>(sel))
  {
    Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*ev);
    auto& tn = Scenario::parentTimeSync(*ev, scenar);
    auto idx = m_root.indexOf(&tn);
    if (idx >= 0)
      return createIndex(idx, 0, (void*)m_root[idx]);
    else
      return QModelIndex{};
  }
  else if (qobject_cast<Scenario::TimeSyncModel*>(sel))
  {
    return QModelIndex{};
  }
  else if (auto st = qobject_cast<Scenario::StateModel*>(sel))
  {
    Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*st);
    auto& tn = Scenario::parentTimeSync(*st, scenar);
    auto it = ossia::find(tn.events(), st->eventId());
    SCORE_ASSERT(it != tn.events().end());
    auto idx = std::distance(tn.events().begin(), it);
    if (idx >= 0)
      return createIndex(idx, 0, (void*)&Scenario::parentEvent(*st, scenar));
    else
      return QModelIndex{};
  }
  else if (auto stp = qobject_cast<Process::ProcessModel*>(sel))
  {
    if (auto state = qobject_cast<Scenario::StateModel*>(stp->parent()))
    {
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*state);
      auto& ev = Scenario::parentEvent(*state, scenar);
      auto it = ossia::find(ev.states(), state->id());
      SCORE_ASSERT(it != ev.states().end());
      auto idx = std::distance(ev.states().begin(), it);

      if (idx >= 0)
        return createIndex(idx, 0, state);
      else
        return QModelIndex{};
    }
    else if (auto cst = qobject_cast<Scenario::IntervalModel*>(stp->parent()))
    {
      auto idx = m_root.indexOf(cst);
      if (idx >= 0)
        return createIndex(idx, 0, (void*)m_root[idx]);
      else
        return QModelIndex{};
    }
  }

  return QModelIndex{};
}

QVariant ObjectItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (role == Qt::DisplayRole)
  {
    if (section == 0)
    {
      return tr("Object");
    }
    return tr("Info");
  }
  else
  {
    return QAbstractItemModel::headerData(section, orientation, role);
  }
}

int ObjectItemModel::rowCount(const QModelIndex& parent) const
{
  auto sel = (QObject*)parent.internalPointer();
  if (isAlive(sel))
  {
    if (auto cst = qobject_cast<Scenario::IntervalModel*>(sel))
    {
      return cst->processes.size();
    }
    else if (auto ev = qobject_cast<Scenario::EventModel*>(sel))
    {
      return ev->states().size();
    }
    else if (auto ts = qobject_cast<Scenario::TimeSyncModel*>(sel))
    {
      return ts->events().size();
    }
    else if (auto st = qobject_cast<Scenario::StateModel*>(sel))
    {
      return st->stateProcesses.size();
    }
    else
    {
      return 0;
    }
  }
  else if (parent == QModelIndex())
  {
    return m_root.size();
  }
  else
  {
    return 0;
  }
}

int ObjectItemModel::columnCount(const QModelIndex& parent) const
{
  return 1;
}

QVariant ObjectItemModel::data(const QModelIndex& index, int role) const
{
  auto sel = (QObject*)index.internalPointer();
  if (!isAlive(sel))
    return {};
  if (role == Qt::SizeHintRole)
  {
    return QSize{200, 20};
  }
  if (role == Qt::FontRole)
  {
    return score::Skin::instance().Medium10Pt;
  }

  if (index.column() == 0)
  {
    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
      if (auto cst = qobject_cast<Scenario::IntervalModel*>(sel))
      {
        return cst->metadata().getName();
      }
      else if (auto ev = qobject_cast<Scenario::EventModel*>(sel))
      {
        return ev->metadata().getName();
      }
      else if (auto tn = qobject_cast<Scenario::TimeSyncModel*>(sel))
      {
        return tn->metadata().getName();
      }
      else if (auto st = qobject_cast<Scenario::StateModel*>(sel))
      {
        return st->metadata().getName();
      }
      else if (auto stp = qobject_cast<Process::ProcessModel*>(sel))
      {
        if (stp->metadata().touchedName())
          return stp->metadata().getName();

        auto name = stp->prettyName();
        if (name.isEmpty())
          name = stp->prettyShortName();
        if (name.isEmpty())
          name = stp->metadata().getName();
        if (name.isEmpty())
          name = "Process";

        return name;
      }
    }
    else if (role == Qt::DecorationRole)
    {
      if (qobject_cast<Scenario::IntervalModel*>(sel))
      {
        static const QIcon icon(":/icons/interval.png");
        return icon;
      }
      else if (auto ev = qobject_cast<Scenario::EventModel*>(sel))
      {
        if (ev->condition() == State::Expression{})
        {
          static const QIcon icon(":/icons/event.png");
          return icon;
        }
        else
        {
          static const QIcon icon(":/icons/cond.png");
          return icon;
        }
      }
      else if (auto tn = qobject_cast<Scenario::TimeSyncModel*>(sel))
      {
        if (!tn->active())
        {
          static const QIcon icon(":/icons/timenode.png");
          return icon;
        }
        else
        {
          static const QIcon icon(":/icons/trigger.png");
          return icon;
        }
      }
      else if (auto st = qobject_cast<Scenario::StateModel*>(sel))
      {
        if (st->messages().rootNode().hasChildren())
        {
          static const QIcon icon(":/icons/state.png");
          return icon;
        }
        else
        {
          static const QIcon icon(":/icons/state_empty.png");
          return icon;
        }
      }
      else if (auto pm = qobject_cast<Process::ProcessModel*>(sel))
      {
        return Process::getCategoryIcon(pm->category());
      }
    }
    else if (role == Qt::ToolTipRole)
    {
      if (auto cst = qobject_cast<Scenario::IntervalModel*>(sel))
      {
        return QString{tr("Start : ") + cst->date().toString()};
      }
      else if (auto ev = qobject_cast<Scenario::EventModel*>(sel))
      {
        return ev->condition().toPrettyString();
      }
      else if (auto tn = qobject_cast<Scenario::TimeSyncModel*>(sel))
      {
        return tn->expression().toPrettyString();
      }
      /*
      else if(auto st = qobject_cast<Scenario::StateModel*>(sel))
      {
        return {};
      }
      else if(auto p = qobject_cast<Process::ProcessModel*>(sel))
      {
        return {};
      }
      */
    }
  }
  return {};
}

Qt::ItemFlags ObjectItemModel::flags(const QModelIndex& index) const
{
  Qt::ItemFlags f = QAbstractItemModel::flags(index);
  f |= Qt::ItemIsSelectable;
  f |= Qt::ItemIsEnabled;
  f |= Qt::ItemIsEditable;
  f |= Qt::ItemIsDropEnabled;

  auto p = (QObject*)index.internalPointer();
  if (qobject_cast<Process::ProcessModel*>(p))
  {
    f |= Qt::ItemIsDragEnabled;
  }

  return f;
}

bool ObjectItemModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if (index.column() == 0)
  {
    auto sel = (QObject*)index.internalPointer();
    if (!isAlive(sel))
      return {};

    CommandDispatcher<> disp{m_ctx.commandStack};
    auto new_name = value.toString();

    auto sanitize = [&](auto* ptr) {
      if (new_name == ptr->metadata().getName())
        return false;
      using T = std::remove_reference_t<decltype(*ptr)>;
      if (new_name.isEmpty())
        new_name = QString("%1.0").arg(Metadata<PrettyName_k, T>::get());
      return true;
    };

    if (auto cst = qobject_cast<Scenario::IntervalModel*>(sel))
    {
      if (!sanitize(cst))
        return false;

      disp.submit<Command::ChangeElementName<Scenario::IntervalModel>>(*cst, new_name);
      return true;
    }
    else if (auto ev = qobject_cast<Scenario::EventModel*>(sel))
    {
      if (!sanitize(ev))
        return false;

      disp.submit<Command::ChangeElementName<Scenario::EventModel>>(*ev, new_name);
      return true;
    }
    else if (auto tn = qobject_cast<Scenario::TimeSyncModel*>(sel))
    {
      if (!sanitize(tn))
        return false;

      disp.submit<Command::ChangeElementName<Scenario::TimeSyncModel>>(*tn, new_name);
      return true;
    }
    else if (auto st = qobject_cast<Scenario::StateModel*>(sel))
    {
      if (!sanitize(st))
        return false;

      disp.submit<Command::ChangeElementName<Scenario::StateModel>>(*st, new_name);
      return true;
    }
    else if (auto p = qobject_cast<Process::ProcessModel*>(sel))
    {
      if (new_name == p->metadata().getName())
        return false;
      if (new_name.isEmpty())
        new_name = QString("%1.0").arg(p->objectName());

      disp.submit<Command::ChangeElementName<Process::ProcessModel>>(*p, new_name);
      return true;
    }
  }
  return false;
}

QMimeData* ObjectItemModel::mimeData(const QModelIndexList& indexes) const
{
  if (indexes.size() != 1)
    return nullptr;

  auto p = (QObject*)indexes.front().internalPointer();
  if (!p)
    return nullptr;

  auto q = qobject_cast<Process::ProcessModel*>(p);
  if (!q)
    return nullptr;

  auto m = new QMimeData;
  m->setData(
      "score/object-item-model-index",
      score::marshall<DataStream>(Path<Process::ProcessModel>{*q}));
  return m;
}

bool ObjectItemModel::canDropMimeData(
    const QMimeData* data,
    Qt::DropAction action,
    int r,
    int column,
    const QModelIndex& parent) const
{
  if (!parent.isValid())
    return false;
  if (r <= 0)
    return false;

  auto row = std::size_t(r);
  auto obj = score::unmarshall<Path<Process::ProcessModel>>(
      data->data("score/object-item-model-index"));
  auto other = obj.try_find(m_ctx);
  if (!other)
    return false;

  auto p = (QObject*)parent.internalPointer();
  Process::ProcessModel* the_proc{};
  if (auto itv = qobject_cast<Scenario::IntervalModel*>(p))
  {
    if (other->parent() != itv)
      return false;
    if (row < itv->processes.size())
    {
      auto pb = itv->processes.map().m_order.begin();
      std::advance(pb, row);

      the_proc = *pb;
      // Row: the row before the process
      if (other == the_proc)
        return false;
    }
    else
    {
      return true;
    }
    return true;
  }
  else if (auto sta = qobject_cast<Scenario::StateModel*>(p))
  {
    if (other->parent() != sta)
      return false;

    if (row < sta->stateProcesses.size())
    {
      auto pb = sta->stateProcesses.map().m_order.begin();
      std::advance(pb, row);

      the_proc = *pb;
      // Row: the row before the process
      if (other == the_proc)
        return false;
    }
    else
    {
      return true;
    }
    return true;
  }
  else
  {
    the_proc = qobject_cast<Process::ProcessModel*>(p);

    if (!the_proc)
      return false;
    if (other == the_proc)
      return false;
    if (other->parent() != the_proc->parent())
      return false;
    return true;
  }

  return true;
}

bool ObjectItemModel::dropMimeData(
    const QMimeData* data,
    Qt::DropAction action,
    int r,
    int column,
    const QModelIndex& parent)
{
  using namespace Scenario::Command;
  if (!parent.isValid())
    return false;
  if (r <= 0)
    return false;

  auto row = std::size_t(r);
  auto obj = score::unmarshall<Path<Process::ProcessModel>>(
      data->data("score/object-item-model-index"));
  auto other = obj.try_find(m_ctx);
  if (!other)
    return false;

  auto p = (QObject*)parent.internalPointer();

  auto move_in_itv = [=](auto itv, std::size_t row) {
    if (other->parent() != itv)
      return false;

    if (row < itv->processes.size())
    {
      auto pb = itv->processes.map().m_order.begin();
      std::advance(pb, row);

      auto the_proc = *pb;
      // Row: the row before the process
      if (other == the_proc)
        return false;

      // put before row: row == 0 -> begin
      CommandDispatcher<> disp{m_ctx.dispatcher};
      disp.submit(new PutProcessBefore{*itv, the_proc->id(), other->id()});
    }
    else if (row >= itv->processes.size())
    {
      // put at end
      CommandDispatcher<> disp{m_ctx.dispatcher};
      disp.submit(new PutProcessBefore{*itv, {}, other->id()});
    }
    return true;
  };

  auto move_in_state = [=](auto sta, std::size_t row) {
    if (other->parent() != sta)
      return false;

    if (row < sta->stateProcesses.size())
    {
      auto pb = sta->stateProcesses.map().m_order.begin();
      std::advance(pb, row);

      auto the_proc = *pb;
      // Row: the row before the process
      if (other == the_proc)
        return false;

      // put before row: row == 0 -> begin
      CommandDispatcher<> disp{m_ctx.dispatcher};
      disp.submit(new PutStateProcessBefore{*sta, the_proc->id(), other->id()});
    }
    else if (row >= sta->stateProcesses.size())
    {
      // put at end
      CommandDispatcher<> disp{m_ctx.dispatcher};
      disp.submit(new PutStateProcessBefore{*sta, {}, other->id()});
    }
    return true;
  };

  if (auto itv = qobject_cast<Scenario::IntervalModel*>(p))
  {
    return move_in_itv(itv, row);
  }
  else if (auto sta = qobject_cast<Scenario::StateModel*>(p))
  {
    return move_in_state(sta, row);
  }
  else
  {
    auto the_proc = qobject_cast<Process::ProcessModel*>(p);

    if (!the_proc)
      return false;
    if (other == the_proc)
      return false;
    if (other->parent() != the_proc->parent())
      return false;

    if (auto itv = qobject_cast<Scenario::IntervalModel*>(the_proc->parent()))
    {
      CommandDispatcher<> disp{m_ctx.dispatcher};
      disp.submit(new PutProcessBefore{*itv, the_proc->id(), other->id()});
    }
    else if (auto sta = qobject_cast<Scenario::StateModel*>(the_proc->parent()))
    {
      CommandDispatcher<> disp{m_ctx.dispatcher};
      disp.submit(new PutStateProcessBefore{*sta, the_proc->id(), other->id()});
    }
    return true;
  }
  return true;
}

Qt::DropActions ObjectItemModel::supportedDropActions() const
{
  return Qt::CopyAction | Qt::MoveAction | Qt::TargetMoveAction;
}

Qt::DropActions ObjectItemModel::supportedDragActions() const
{
  return Qt::CopyAction | Qt::MoveAction | Qt::TargetMoveAction;
}

SelectionStackWidget::SelectionStackWidget(
    score::SelectionStack& s,
    QWidget* parent,
    ObjectWidget* objects)
    : QWidget{parent}, m_stack{s}, m_selector{s, objects}
{
  m_prev = new score::ArrowButton{Qt::LeftArrow, this};
  m_prev->setEnabled(m_stack.canUnselect());
  m_prev->setStatusTip(tr("Previous selection."));

  m_label = new TextLabel{"History", this};
  m_label->setStatusTip(tr("Go back and forth in the selected items."));

  m_next = new score::ArrowButton{Qt::RightArrow, this};
  m_next->setEnabled(m_stack.canReselect());
  m_next->setStatusTip(tr("Next selection."));

  m_left = new score::ArrowButton{Qt::LeftArrow, this};
  m_left->setEnabled(m_selector.hasLeft());
  m_left->setStatusTip(tr("Select the item to the left."));

  m_right = new score::ArrowButton{Qt::RightArrow, this};
  m_right->setEnabled(m_selector.hasRight());
  m_right->setStatusTip(tr("Select the item to the right."));

  m_up = new score::ArrowButton{Qt::UpArrow, this};
  m_up->setEnabled(m_selector.hasUp());
  m_up->setStatusTip(tr("Select the item above."));

  m_down = new score::ArrowButton{Qt::DownArrow, this};
  m_down->setEnabled(m_selector.hasDown());
  m_down->setStatusTip(tr("Select the item below."));

  auto lay = new score::MarginLess<QHBoxLayout>{this};
  lay->setSizeConstraint(QLayout::SetMinimumSize);
  lay->addWidget(m_prev, 0, Qt::AlignVCenter);
  lay->addWidget(m_label);
  lay->addWidget(m_next, 0, Qt::AlignVCenter);
  lay->addStretch(8);
  lay->addWidget(m_left, 0, Qt::AlignVCenter);
  lay->addWidget(m_up, 0, Qt::AlignVCenter);
  lay->addWidget(m_down, 0, Qt::AlignVCenter);
  lay->addWidget(m_right, 0, Qt::AlignVCenter);
  setLayout(lay);

  connect(m_prev, &QToolButton::pressed, [&]() { m_stack.unselect(); });
  connect(m_next, &QToolButton::pressed, [&]() { m_stack.reselect(); });

  connect(m_left, &QToolButton::pressed, [&]() { m_selector.selectLeft(); });
  connect(m_right, &QToolButton::pressed, [&]() { m_selector.selectRight(); });
  connect(m_up, &QToolButton::pressed, [&]() { m_selector.selectUp(); });
  connect(m_down, &QToolButton::pressed, [&]() { m_selector.selectDown(); });

  con(m_stack, &score::SelectionStack::currentSelectionChanged, this, [=] {
    m_prev->setEnabled(m_stack.canUnselect());
    m_next->setEnabled(m_stack.canReselect());
    m_left->setEnabled(m_selector.hasLeft());
    m_right->setEnabled(m_selector.hasRight());
    m_up->setEnabled(m_selector.hasUp());
    m_down->setEnabled(m_selector.hasDown());
  });
}

ObjectPanelDelegate::ObjectPanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}
    , m_widget{new SizePolicyWidget}
    , m_lay{new QVBoxLayout{m_widget}}
    , m_searchWidget{new SearchWidget{ctx}}
{
  m_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  m_widget->setMinimumHeight(160);
  m_widget->setSizeHint({250, 100});
  m_widget->setMinimumWidth(250);
  m_widget->setStatusTip(
      QObject::tr("Shows the currently selected items.\n"
                  "They can be renamed by double-clicking.\n"
                  "More options are available on the right-click menu."));
}

QWidget* ObjectPanelDelegate::widget()
{
  return m_widget;
}

const score::PanelStatus& ObjectPanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{
      true,
      false,
      Qt::RightDockWidgetArea,
      8,
      QObject::tr("Objects"),
      "objects",
      QObject::tr("Ctrl+Shift+O")};

  return status;
}

void ObjectPanelDelegate::on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm)
{
  using namespace score;
  delete m_objects;
  m_objects = nullptr;

  delete m_stack;
  m_stack = nullptr;

  m_lay->removeWidget(m_searchWidget);

  if (newm)
  {
    m_objects = new ObjectWidget{*newm, m_widget};

    m_objects->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);

    SelectionStack& stack = newm->selectionStack;
    m_stack = new SelectionStackWidget{stack, m_widget, m_objects};

    m_lay->addWidget(m_stack);
    m_lay->addWidget(m_searchWidget);
    m_lay->addWidget(m_objects);

    setNewSelection(stack.currentSelection());
  }
}

void ObjectPanelDelegate::setNewSelection(const Selection& sel)
{
  if (m_objects)
  {
    m_objects->model.setSelected(sel.toList());
    m_objects->expandAll();

    auto cur_sel = document()->selectionStack.currentSelection();
    auto idx = m_objects->model.index(0, 0, {});

    auto selection = m_objects->selectionModel();
    m_objects->updatingSelection = true;

    QItemSelection toSelect, toDeselect;
    while (idx.isValid())
    {
      auto ptr = idx.internalPointer();
      if (cur_sel.contains((IdentifiedObjectAbstract*)ptr))
      {
        toSelect.append(QItemSelectionRange{idx, idx});
      }
      else
      {
        toDeselect.append(QItemSelectionRange{idx, idx});
      }
      idx = m_objects->indexBelow(idx);
    }
    selection->select(toSelect, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    selection->select(toDeselect, QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
    m_objects->updatingSelection = false;

    m_objects->header()->resizeSection(1, QHeaderView::Stretch);
    m_objects->header()->resizeSection(0, QHeaderView::ResizeToContents);
    if (m_objects->header()->sectionSize(0) < 140)
      m_objects->header()->resizeSection(0, 140);
  }
}

ObjectWidget::ObjectWidget(const score::DocumentContext& ctx, QWidget* par)
    : QTreeView{par}, model{ctx, this}, m_ctx{ctx}
{
  setModel(&model);

  setAnimated(true);
  setAlternatingRowColors(true);
  setMidLineWidth(40);
  setUniformRowHeights(true);
  setWordWrap(false);
  setMouseTracking(true);
  setDragDropMode(QAbstractItemView::DragDrop);
  header()->hide();

  con(model, &ObjectItemModel::changed, this, &QTreeView::expandAll);
}

void ObjectWidget::selectionChanged(
    const QItemSelection& selected,
    const QItemSelection& deselected)
{
  if ((selected.size() > 0 || deselected.size() > 0) && !updatingSelection)
  {
    score::SelectionDispatcher d{m_ctx.selectionStack};
    auto sel = this->selectedIndexes();
    if (!sel.empty())
    {
      auto obj = (IdentifiedObjectAbstract*)sel.at(0).internalPointer();
      d.select(Selection{obj});
    }
    else
    {
      d.deselect();
    }
  }
}

struct ContextMenuCallback
{
  const score::DocumentContext& m_ctx;
  const IntervalModel& cst;
  void operator()(const AddProcessDialog::Key& proc, QString dat)
  {
    using namespace Scenario::Command;
    Macro m{new AddProcessInNewSlot, m_ctx};

    if (auto p = m.createProcess(cst, proc, dat, {}))
    {
      m.addLayerInNewSlot(cst, *p);
      m.commit();
    }
  }
};

void ObjectWidget::contextMenuEvent(QContextMenuEvent* ev)
{
  auto point = ev->pos();
  auto index = indexAt(point);
  if (index.isValid())
  {
    auto ptr = (QObject*)index.internalPointer();
    if (!ptr)
      return;

    QMenu* m = new QMenu{this};

    if (auto cst = qobject_cast<Scenario::IntervalModel*>(ptr))
    {
      auto addproc = new QAction{tr("Add process"), m};
      m->addAction(addproc);
      connect(addproc, &QAction::triggered, this, [this, &cst] {
        auto& fact = m_ctx.app.interfaces<Process::ProcessFactoryList>();
        auto dialog = new AddProcessDialog{fact, Process::ProcessFlags::SupportsTemporal, this};
        dialog->on_okPressed = ContextMenuCallback{m_ctx, *cst};

        dialog->launchWindow();
        dialog->deleteLater();
      });
    }
    else if (auto state = qobject_cast<Scenario::StateModel*>(ptr))
    {
      auto addproc = new QAction{tr("Add process"), m};
      m->addAction(addproc);
      connect(addproc, &QAction::triggered, this, [=] {
        auto& fact = m_ctx.app.interfaces<Process::ProcessFactoryList>();
        auto dialog = new AddProcessDialog{fact, Process::ProcessFlags::SupportsState, this};

        dialog->on_okPressed = [&](const auto& proc, QString dat) {
          CommandDispatcher<> disp{m_ctx.commandStack};
          disp.submit<Scenario::Command::AddStateProcessToState>(*state, proc);
        };

        dialog->launchWindow();
        dialog->deleteLater();
      });
    }
    else if (auto proc = qobject_cast<Process::ProcessModel*>(ptr))
    {
      if (auto state = qobject_cast<Scenario::StateModel*>(proc->parent()))
      {
        auto deleteact = new QAction{tr("Remove"), m};
        m->addAction(deleteact);
        connect(deleteact, &QAction::triggered, this, [=] {
          CommandDispatcher<> c{m_ctx.commandStack};
          c.submit<Scenario::Command::RemoveStateProcess>(*state, proc->id());
        });
      }
      else if (auto itv = qobject_cast<Scenario::IntervalModel*>(proc->parent()))
      {
        auto deleteact = new QAction{tr("Remove"), m};
        m->addAction(deleteact);
        connect(deleteact, &QAction::triggered, this, [=] {
          CommandDispatcher<> c{m_ctx.commandStack};
          c.submit<Scenario::Command::RemoveProcessFromInterval>(*itv, proc->id());
        });
        auto duplicate = new QAction{tr("Duplicate"), m};
        m->addAction(duplicate);
        connect(duplicate, &QAction::triggered, this, [=] {
          CommandDispatcher<> c{m_ctx.commandStack};
          c.submit<Scenario::Command::DuplicateOnlyProcessToInterval>(*itv, *proc);
        });
      }
    }

    m->exec(mapToGlobal(point));
    m->deleteLater();
  }
}

NeighbourSelector::NeighbourSelector(score::SelectionStack& s, ObjectWidget* objects)
    : m_stack{s}, m_objects{objects}, m_selectionDispatcher{s}
{
}

bool NeighbourSelector::hasLeft() const
{
  for (const auto& obj : m_stack.currentSelection())
  {
    if (qobject_cast<const IntervalModel*>(obj.data()))
    {
      // Interval always have previous state
      return true;
    }
    else if (auto state = qobject_cast<const StateModel*>(obj.data()))
    {
      if (state->previousInterval())
        return true;
    }
  }
  return false;
}

bool NeighbourSelector::hasRight() const
{
  for (const auto& obj : m_stack.currentSelection())
  {
    if (qobject_cast<const IntervalModel*>(obj.data()))
    {
      // Interval always have previous state
      return true;
    }
    else if (auto state = qobject_cast<const StateModel*>(obj.data()))
    {
      if (state->nextInterval())
        return true;
    }
  }
  return false;
}

bool NeighbourSelector::hasUp() const
{
  auto cur_idx = m_objects->selectionModel()->currentIndex();
  auto idx = m_objects->indexAbove(cur_idx);
  bool res = idx.isValid();
  return res;
}

bool NeighbourSelector::hasDown() const
{
  auto cur_idx = m_objects->selectionModel()->currentIndex();
  auto idx = m_objects->indexBelow(cur_idx);
  bool res = idx.isValid();
  return res;
}

void NeighbourSelector::selectRight()
{
  Selection sel{};

  for (const auto& obj : m_stack.currentSelection())
  {
    if (auto interval = qobject_cast<const IntervalModel*>(obj.data()))
    {
      // Interval always have previous state
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*interval);
      sel.append(scenar.state(interval->endState()));
    }
    else if (auto state = qobject_cast<const StateModel*>(obj.data()))
    {
      if (state->nextInterval())
      {
        Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*state);
        sel.append(scenar.interval(*state->nextInterval()));
      }
    }
  }

  if (!sel.empty())
    m_selectionDispatcher.select(sel);
}

void NeighbourSelector::selectLeft()
{
  Selection sel{};

  for (const auto& obj : m_stack.currentSelection())
  {
    if (auto interval = qobject_cast<const IntervalModel*>(obj.data()))
    {
      // Interval always have previous state
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*interval);
      sel.append(scenar.state(interval->startState()));
    }
    else if (auto state = qobject_cast<const StateModel*>(obj.data()))
    {
      if (state->previousInterval())
      {
        Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*state);
        sel.append(scenar.interval(*state->previousInterval()));
      }
    }
  }

  if (!sel.empty())
    m_selectionDispatcher.select(sel);
}

void NeighbourSelector::selectUp()
{
  auto cur_idx = m_objects->selectionModel()->currentIndex();
  auto idx = m_objects->indexAbove(cur_idx);
  if (idx.isValid())
  {
    Selection sel{};
    sel.append((IdentifiedObjectAbstract*)idx.internalPointer());
    m_selectionDispatcher.select(sel);
  }
}

void NeighbourSelector::selectDown()
{
  auto cur_idx = m_objects->selectionModel()->currentIndex();
  auto idx = m_objects->indexBelow(cur_idx);
  if (idx.isValid())
  {
    Selection sel{};
    sel.append((IdentifiedObjectAbstract*)idx.internalPointer());
    m_selectionDispatcher.select(sel);
  }
}

SearchWidget::SearchWidget(const score::GUIApplicationContext& ctx)
    : score::SearchLineEdit{nullptr}, m_ctx{ctx}
{
  setAcceptDrops(true);
  const auto& appCtx = score::GUIAppContext();

  for (auto& cpt : appCtx.panels())
  {
    if (Explorer::PanelDelegate* panel = dynamic_cast<Explorer::PanelDelegate*>(&cpt))
    {
      Explorer::DeviceExplorerWidget* widget
          = static_cast<Explorer::DeviceExplorerWidget*>(panel->widget());
      connect(
          widget,
          &Explorer::DeviceExplorerWidget::findAddresses,
          this,
          &SearchWidget::on_findAddresses);
    }
  }
}
template <typename T>
void add_if_contains(const T& o, const QString& str, Selection& sel)
{
  const auto& obj = o.metadata();
  if (obj.getName().contains(str) || obj.getComment().contains(str)
      || obj.getLabel().contains(str))
  {
    sel.append(o);
  }
}
void add_if_contains(const Scenario::CommentBlockModel& o, const QString& str, Selection& sel)
{
  if (o.content().contains(str))
  {
    sel.append(o);
  }
}

void SearchWidget::on_findAddresses(QStringList strlst)
{
  QString searchTxt = "address=";
  for (auto str : strlst)
  {
    searchTxt += str;
    if (str != strlst.back())
      searchTxt += ",";
  }
  setText(searchTxt);
  search();
}

void SearchWidget::search()
{
  const QString& stxt = text();
  std::vector<State::AddressAccessor> addresses;

  int idx = stxt.indexOf("=");
  if (idx >= 0)
  {
    QString substr = stxt.mid(0, idx);
    if (substr == "address")
    {
      QString addrstr = stxt.mid(idx + 1);
      if (auto spaceidx = addrstr.indexOf(" ") >= 0)
        addrstr = substr.mid(0, spaceidx);

      int comma = addrstr.indexOf(",");
      int offset = 0;
      while (comma >= 0)
      {
        auto sub = addrstr.mid(offset, comma);
        auto optaddr = State::parseAddressAccessor(sub);
        if (optaddr)
          addresses.push_back(*optaddr);
        offset = comma + 1;
        comma = addrstr.indexOf(",", offset);
      }
      auto sub = addrstr.mid(offset, comma);
      auto optaddr = State::parseAddressAccessor(sub);
      if (optaddr)
        addresses.push_back(*optaddr);
    }
  }

  if (addresses.empty())
  {
    auto opt = State::parseAddressAccessor(stxt);
    if (opt)
      addresses.push_back(*opt);
  }

  auto* doc = m_ctx.documents.currentDocument();

  auto scenarioModel = doc->focusManager().get();
  Selection sel{};

  if (scenarioModel)
  {
    // Serialize ALL the things
    for (const auto& obj : scenarioModel->children())
    {
      if (auto state = qobject_cast<const StateModel*>(obj))
      {
        bool flag = false; // used to break loop at several point to avoid adding
                           // the same object severals time and to sped-up main loop

        State::MessageList list = Process::flatten(state->messages().rootNode());

        for (const auto& addr : addresses)
        {
          auto nodes = Process::try_getNodesFromAddress(state->messages().rootNode(), addr);

          if (!nodes.empty())
          {
            sel.append(state);
            flag = true;
            continue;
          }

          for (auto mess : list)
          {
            if (mess.address.address.toString().contains(addr.address.toString()))
            {
              sel.append(state);
              flag = true;
              continue;
            }
          }
          if (flag)
            continue;
        }
        if (flag)
          continue;
        for (auto mess : list)
        {
          if (mess.address.address.toString().contains(stxt))
          {
            sel.append(state);
            flag = true;
            continue;
          }
        }
        if (flag)
          continue;
        add_if_contains(*state, stxt, sel);
      }
      else if (auto event = qobject_cast<const EventModel*>(obj))
      {
        add_if_contains(*event, stxt, sel);
      }
      else if (auto ts = qobject_cast<const TimeSyncModel*>(obj))
      {
        add_if_contains(*ts, stxt, sel);
      }
      else if (auto cmt = qobject_cast<const CommentBlockModel*>(obj))
      {
        add_if_contains(*cmt, stxt, sel);
      }
      else if (auto interval = qobject_cast<const IntervalModel*>(obj))
      {
        add_if_contains(*interval, stxt, sel);
      }
    }
  }

  score::SelectionDispatcher d{doc->context().selectionStack};
  d.select(sel);
}

void SearchWidget::dragEnterEvent(QDragEnterEvent* event)
{
  const auto& formats = event->mimeData()->formats();
  if (formats.contains(score::mime::messagelist()))
  {
    event->accept();
  }
}

void SearchWidget::dropEvent(QDropEvent* ev)
{
  auto& mime = *ev->mimeData();

  // TODO refactor this with AutomationPresenter and AddressLineEdit
  if (mime.formats().contains(score::mime::nodelist()))
  {
    Mime<Device::FreeNodeList>::Deserializer des{mime};
    Device::FreeNodeList nl = des.deserialize();
    if (nl.empty())
      return;

    // We only take the first node.
    const Device::Node& node = nl.front().second;
    // TODO refactor with CreateCurves and AutomationDropHandle
    if (node.is<Device::AddressSettings>())
    {
      const Device::AddressSettings& addr = node.get<Device::AddressSettings>();
      Device::FullAddressSettings as;
      static_cast<Device::AddressSettingsCommon&>(as) = addr;
      as.address = nl.front().first;

      setText(as.address.toString());
      returnPressed();
    }
  }
  else if (mime.formats().contains(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    State::MessageList ml = des.deserialize();
    if (!ml.empty())
    {
      setText(ml[0].address.toString());
      returnPressed();
    }
  }
}
}

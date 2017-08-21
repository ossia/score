#include "ObjectItemModel.hpp"

#include <Scenario/Process/ScenarioInterface.hpp>
#include <Process/Process.hpp>
#include <Process/StateProcess.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
namespace Scenario
{

ObjectItemModel::ObjectItemModel(QObject* parent)
  : QAbstractItemModel{parent}
{
}

void ObjectItemModel::setSelected(QList<const IdentifiedObjectAbstract*> objs)
{
  QList<const QObject*> root{};
  for(const QObject* sel : objs)
  {
    if(auto cst = dynamic_cast<const Scenario::ConstraintModel*>(sel))
    {
      root.push_back(cst);
    }
    else if(auto ev = dynamic_cast<const Scenario::EventModel*>(sel))
    {
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*ev);
      root.push_back(&Scenario::parentTimeNode(*ev, scenar));
    }
    else if(auto tn = dynamic_cast<const Scenario::TimeNodeModel*>(sel))
    {
      root.push_back(tn);
    }
    else if(auto st = dynamic_cast<const Scenario::StateModel*>(sel))
    {
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*st);
      root.push_back(&Scenario::parentTimeNode(*st, scenar));
    }
    else if(auto stp = dynamic_cast<const Process::StateProcess*>(sel))
    {
      auto state = static_cast<Scenario::StateModel*>(stp->parent());
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*state);
      root.push_back(&Scenario::parentTimeNode(*state, scenar));
    }
    else if(auto p = dynamic_cast<const Process::ProcessModel*>(sel))
    {
      root.push_back(p->parent());
    }
  }

  root = root.toSet().toList();
  if(root != m_root)
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
  if(m_root.empty())
    return;

  for(auto obj : m_root)
  {
    bool is_cst = dynamic_cast<const Scenario::ConstraintModel*>(obj);
    if(is_cst)
    {
      auto cst = static_cast<const Scenario::ConstraintModel*>(obj);
      cst->processes.added.connect<ObjectItemModel, &ObjectItemModel::recompute>(*this);
      cst->processes.removing.connect<ObjectItemModel, &ObjectItemModel::recompute>(*this);
    }
    else
    {
      auto tn = static_cast<const Scenario::TimeNodeModel*>(obj);
      auto& scenar = Scenario::parentScenario(*tn);
      m_itemCon.push_back(connect(tn, &TimeNodeModel::newEvent, this, [=] { recompute(); }));
      m_itemCon.push_back(connect(tn, &TimeNodeModel::eventRemoved, this, [=] { recompute(); }));

      for(const auto& ev : tn->events())
      {
        auto& e = scenar.event(ev);
        m_itemCon.push_back(con(e, &EventModel::statesChanged, this, [=] { recompute(); }));
        for(const auto& st : e.states())
        {
          auto& s = scenar.state(st);
          s.stateProcesses.added.connect<ObjectItemModel, &ObjectItemModel::recompute>(*this);
          s.stateProcesses.removed.connect<ObjectItemModel, &ObjectItemModel::recompute>(*this);
        }
      }
    }

    m_itemCon.push_back(
          connect(obj, &QObject::destroyed,
                  this, [=] {
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
  for(auto& con : m_itemCon)
    QObject::disconnect(con);
  m_itemCon.clear();
}

QModelIndex ObjectItemModel::index(int row, int column, const QModelIndex& parent) const
{
  auto sel = (QObject*)parent.internalPointer();
  if(sel)
  {
    if(auto cst = dynamic_cast<Scenario::ConstraintModel*>(sel))
    {
      auto it = cst->processes.begin();
      std::advance(it, row);
      return createIndex(row, column, &*(it));
    }
    else if(auto ev = dynamic_cast<Scenario::EventModel*>(sel))
    {
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*ev);
      auto it = ev->states().begin();
      std::advance(it, row);
      auto st = scenar.findState(*it);

      return createIndex(row, column, st);
    }
    else if(auto tn = dynamic_cast<Scenario::TimeNodeModel*>(sel))
    {
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*tn);
      auto it = tn->events().begin();
      std::advance(it, row);
      auto ev = scenar.findEvent(*it);
      return createIndex(row, column, ev);
    }
    else if(auto st = dynamic_cast<Scenario::StateModel*>(sel))
    {
      auto it = st->stateProcesses.begin();
      std::advance(it, row);
      auto& proc = *it;
      return createIndex(row, column, &proc);
    }
    else
    {
      return QModelIndex{};
    }
  }
  else if(!m_root.empty() && row >= 0)
  {
    return createIndex(row, column, (void*)m_root[row]);
  }
  else
  {
    return QModelIndex{};
  }
}

QModelIndex ObjectItemModel::parent(const QModelIndex& child) const
{
  auto sel = (QObject*)child.internalPointer();
  if(sel == nullptr)
    return QModelIndex{};


  if(dynamic_cast<Scenario::ConstraintModel*>(sel))
  {
    return QModelIndex{};
  }
  else if(auto ev = dynamic_cast<Scenario::EventModel*>(sel))
  {
    Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*ev);
    auto& tn = Scenario::parentTimeNode(*ev, scenar);
    auto idx = m_root.indexOf(&tn);
    return createIndex(0, 0, (void*)m_root[idx]);
  }
  else if(dynamic_cast<Scenario::TimeNodeModel*>(sel))
  {
    return QModelIndex{};
  }
  else if(auto st = dynamic_cast<Scenario::StateModel*>(sel))
  {
    Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*st);
    auto& tn = Scenario::parentTimeNode(*st, scenar);
    auto it = ossia::find(tn.events(), st->eventId());
    ISCORE_ASSERT(it != tn.events().end());
    auto idx = std::distance(tn.events().begin(), it);

    return createIndex(idx, 0, (void*)&Scenario::parentEvent(*st, scenar));
  }
  else if(auto stp = dynamic_cast<Process::StateProcess*>(sel))
  {
    auto state = static_cast<Scenario::StateModel*>(stp->parent());
    Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*state);
    auto& ev = Scenario::parentEvent(*st, scenar);
    auto it = ossia::find(ev.states(), state->id());
    ISCORE_ASSERT(it != ev.states().end());
    auto idx = std::distance(ev.states().begin(), it);

    return createIndex(idx, 0, state);
  }
  else if(auto proc = dynamic_cast<Process::ProcessModel*>(sel))
  {
    auto idx = m_root.indexOf(proc->parent());
    return createIndex(idx, 0, (void*)m_root[idx]);
  }

  return QModelIndex{};
}

QVariant ObjectItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if(role == Qt::DisplayRole)
  {
    if(section == 0)
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
  if(sel)
  {
    if(auto cst = dynamic_cast<Scenario::ConstraintModel*>(sel))
    {
      return cst->processes.size();
    }
    else if(auto ev = dynamic_cast<Scenario::EventModel*>(sel))
    {
      return ev->states().size();
    }
    else if(auto tn = dynamic_cast<Scenario::TimeNodeModel*>(sel))
    {
      return tn->events().size();
    }
    else if(auto st = dynamic_cast<Scenario::StateModel*>(sel))
    {
      return st->stateProcesses.size();
    }
    else
    {
      return 0;
    }
  }

  return m_root.size();
}

int ObjectItemModel::columnCount(const QModelIndex& parent) const
{
  return 1;
}

QVariant ObjectItemModel::data(const QModelIndex& index, int role) const
{
  auto sel = (QObject*)index.internalPointer();
  if(!sel)
    return {};
  if(role == Qt::SizeHintRole)
  {
    return QSize{200, 20};
  }
  if(role == Qt::FontRole)
  {
    return ScenarioStyle::instance().Bold10Pt;
  }

  if(index.column() == 0)
  {
    if(role == Qt::DisplayRole)
    {
      if(auto cst = dynamic_cast<Scenario::ConstraintModel*>(sel))
      {
        return cst->metadata().getName();
      }
      else if(auto ev = dynamic_cast<Scenario::EventModel*>(sel))
      {
        return ev->metadata().getName();
      }
      else if(auto tn = dynamic_cast<Scenario::TimeNodeModel*>(sel))
      {
        return tn->metadata().getName();
      }
      else if(auto st = dynamic_cast<Scenario::StateModel*>(sel))
      {
        return st->metadata().getName();
      }
      else if(auto stp = dynamic_cast<Process::StateProcess*>(sel))
      {
        auto name = stp->prettyName();
        if(name.isEmpty())
          name = stp->prettyShortName();
        return name;
      }
      else if(auto p = dynamic_cast<Process::ProcessModel*>(sel))
      {
        auto name = p->prettyName();
        if(name.isEmpty())
          name = p->prettyShortName();
        return name;
      }
    }
    else if(role == Qt::DecorationRole)
    {
      if(dynamic_cast<Scenario::ConstraintModel*>(sel))
      {
        static const QIcon icon(":/images/constraint.svg");
        return icon;
      }
      else if(auto ev = dynamic_cast<Scenario::EventModel*>(sel))
      {
        if(ev->condition() == State::Expression{})
        {
          static const QIcon icon(":/images/event.svg");
          return icon;
        }
        else
        {
          static const QIcon icon(":/images/cond.svg");
          return icon;
        }
      }
      else if(auto tn = dynamic_cast<Scenario::TimeNodeModel*>(sel))
      {
        if(!tn->active())
        {
          static const QIcon icon(":/images/timenode.svg");
          return icon;
        }
        else
        {
          static const QIcon icon(":/images/trigger.svg");
          return icon;
        }
      }
      else if(auto st = dynamic_cast<Scenario::StateModel*>(sel))
      {
        if(st->messages().rootNode().hasChildren())
        {
          static const QIcon icon(":/images/state.svg");
          return icon;
        }
        else
        {
          static const QIcon icon(":/images/state-empty.svg");
          return icon;
        }
      }
      else if(dynamic_cast<Process::StateProcess*>(sel))
      {
        static const QIcon icon(":/images/process.svg");
        return icon;
      }
      else if(dynamic_cast<Process::ProcessModel*>(sel))
      {
        static const QIcon icon(":/images/process.svg");
        return icon;
      }
    }
    else if(role == Qt::ToolTipRole)
    {
      if(auto cst = dynamic_cast<Scenario::ConstraintModel*>(sel))
      {
        return tr("Start : ") + cst->startDate().toString();
      }
      else if(auto ev = dynamic_cast<Scenario::EventModel*>(sel))
      {
        return ev->condition().toPrettyString();
      }
      else if(auto tn = dynamic_cast<Scenario::TimeNodeModel*>(sel))
      {
        return tn->expression().toPrettyString();
      }
      else if(auto st = dynamic_cast<Scenario::StateModel*>(sel))
      {
        return {};
      }
      else if(auto stp = dynamic_cast<Process::StateProcess*>(sel))
      {
        return {};
      }
      else if(auto p = dynamic_cast<Process::ProcessModel*>(sel))
      {
        return {};
      }
    }
  }
  return {};
}

Qt::ItemFlags ObjectItemModel::flags(const QModelIndex& index) const
{
  return QAbstractItemModel::flags(index);
  Qt::ItemFlags f{};
  f |= Qt::ItemIsSelectable;
  f |= Qt::ItemIsEnabled;
  return f;
}



ObjectPanelDelegate::ObjectPanelDelegate(const iscore::GUIApplicationContext &ctx)
  : iscore::PanelDelegate{ctx}
  , m_widget{new SizePolicyWidget}
  , m_lay{new iscore::MarginLess<QVBoxLayout>{m_widget}}
{
  m_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  m_widget->setMinimumHeight(100);
  m_widget->setSizeHint({200, 100});
}

QWidget *ObjectPanelDelegate::widget()
{
  return m_widget;
}

const iscore::PanelStatus &ObjectPanelDelegate::defaultPanelStatus() const
{
  static const iscore::PanelStatus status{true, Qt::RightDockWidgetArea, 8,
        QObject::tr("Objects"),
        QObject::tr("Ctrl+Shift+O")};

  return status;
}

void ObjectPanelDelegate::on_modelChanged(iscore::MaybeDocument oldm, iscore::MaybeDocument newm)
{
  using namespace iscore;
  delete m_objects;
  m_objects = nullptr;
  if (newm)
  {
    m_objects = new ObjectWidget{*newm, m_widget};

    m_objects->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    m_lay->addWidget(m_objects);

    setNewSelection(newm->selectionStack.currentSelection());
  }
}

void ObjectPanelDelegate::setNewSelection(const Selection &sel)
{
  if (m_objects)
  {
    m_objects->model.setSelected(sel.toList());
    m_objects->expandAll();

    auto cur_sel = document()->selectionStack.currentSelection();
    auto idx = m_objects->model.index(0, 0, {});

    auto selection = m_objects->selectionModel();
    m_objects->updatingSelection = true;
    while(idx.isValid())
    {
      auto ptr = idx.internalPointer();
      if(cur_sel.contains((IdentifiedObjectAbstract*)ptr))
      {
        selection->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
      }
      else
      {
        selection->select(idx, QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
      }
      idx = m_objects->indexBelow(idx);
    }
    m_objects->updatingSelection = false;

    m_objects->header()->resizeSection(1, QHeaderView::Stretch);
    m_objects->header()->resizeSection(0, QHeaderView::ResizeToContents);
    if(m_objects->header()->sectionSize(0) < 140)
      m_objects->header()->resizeSection(0, 140);

  }
}

}

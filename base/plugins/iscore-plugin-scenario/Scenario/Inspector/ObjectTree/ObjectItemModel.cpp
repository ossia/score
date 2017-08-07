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
namespace Scenario
{

ObjectItemModel::ObjectItemModel(QObject* parent)
  : QAbstractItemModel{parent}
{
}

void ObjectItemModel::setSelected(const QObject* sel)
{
  const QObject* root{};
  if(auto cst = dynamic_cast<const Scenario::ConstraintModel*>(sel))
  {
    root = cst;
  }
  else if(auto ev = dynamic_cast<const Scenario::EventModel*>(sel))
  {
    Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*ev);
    root = &Scenario::parentTimeNode(*ev, scenar);
  }
  else if(auto tn = dynamic_cast<const Scenario::TimeNodeModel*>(sel))
  {
    root = tn;
  }
  else if(auto st = dynamic_cast<const Scenario::StateModel*>(sel))
  {
    Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*st);
    root = &Scenario::parentTimeNode(*st, scenar);
  }
  else if(auto stp = dynamic_cast<const Process::StateProcess*>(sel))
  {
    auto state = static_cast<Scenario::StateModel*>(stp->parent());
    Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*state);
    root = &Scenario::parentTimeNode(*state, scenar);
  }
  else if(auto p = dynamic_cast<const Process::ProcessModel*>(sel))
  {
    root = p->parent();
  }

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
  if(!m_root)
    return;

  bool is_cst = dynamic_cast<const Scenario::ConstraintModel*>(m_root);
  if(is_cst)
  {
    auto cst = static_cast<const Scenario::ConstraintModel*>(m_root);
    cst->processes.added.connect<ObjectItemModel, &ObjectItemModel::recompute>(*this);
    cst->processes.removing.connect<ObjectItemModel, &ObjectItemModel::recompute>(*this);
  }
  else
  {
    auto tn = static_cast<const Scenario::TimeNodeModel*>(m_root);
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

  m_con = connect(m_root, &QObject::destroyed,
          this, [=] { m_root = nullptr; cleanConnections(); });
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
  else
  {
    return createIndex(row, column, (void*)m_root);
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
  else if(dynamic_cast<Scenario::EventModel*>(sel))
  {
    return createIndex(0, 0, (void*)m_root);
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
  else if(dynamic_cast<Process::ProcessModel*>(sel))
  {
    return createIndex(0, 0, (void*)m_root);
  }

  return QModelIndex{};
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
  }
  else if(m_root)
  {
    return 1;
  }
  return 0;
}

int ObjectItemModel::columnCount(const QModelIndex& parent) const
{
  return 1;
}

QVariant ObjectItemModel::data(const QModelIndex& index, int role) const
{
  auto sel = (QObject*)index.internalPointer();
  if(sel && role == Qt::DisplayRole)
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
  else if(role == Qt::SizeHintRole)
  {
    return QSize{200, 20};
  }
  else if(role == Qt::FontRole)
  {
    return ScenarioStyle::instance().Bold10Pt;
  }
  else if(role == Qt::DecorationRole)
  {
    if(dynamic_cast<Scenario::ConstraintModel*>(sel))
    {
      static const QIcon icon(":/images/constraint.svg");
      return icon;
    }
    else if(dynamic_cast<Scenario::EventModel*>(sel))
    {
      static const QIcon icon(":/images/cond.svg");
      return icon;
    }
    else if(dynamic_cast<Scenario::TimeNodeModel*>(sel))
    {
      static const QIcon icon(":/images/trigger.svg");
      return icon;
    }
    else if(dynamic_cast<Scenario::StateModel*>(sel))
    {
      static const QIcon icon(":/images/state.svg");
      return icon;
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

}

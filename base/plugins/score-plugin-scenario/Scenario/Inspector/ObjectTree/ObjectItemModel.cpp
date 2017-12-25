#include "ObjectItemModel.hpp"

#include <Scenario/Process/ScenarioInterface.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/StateProcess.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Commands/Interval/CreateProcessInNewSlot.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Process/StateProcessFactoryList.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <QToolButton>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <Scenario/Commands/State/AddStateProcess.hpp>
#include <Scenario/Commands/State/RemoveStateProcess.hpp>
#include <Scenario/DialogWidget/AddProcessDialog.hpp>
#include <score/widgets/TextLabel.hpp>

// SearchWidget
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <QJsonObject>
#include <QJsonDocument>
#include <core/presenter/DocumentManager.hpp>
#include <State/MessageListSerialization.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <Explorer/Panel/DeviceExplorerPanelDelegate.hpp>
#include <Scenario/Commands/Metadata/ChangeElementName.hpp>

namespace Scenario
{

ObjectItemModel::ObjectItemModel(const score::DocumentContext& ctx, QObject* parent)
  : QAbstractItemModel{parent}
  , m_ctx{ctx}
{
}

void ObjectItemModel::setSelected(QList<const IdentifiedObjectAbstract*> objs)
{
  QList<const QObject*> root{};
  for(const QObject* sel : objs)
  {
    if(auto cst = dynamic_cast<const Scenario::IntervalModel*>(sel))
    {
      root.push_back(cst);
    }
    else if(auto ev = dynamic_cast<const Scenario::EventModel*>(sel))
    {
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*ev);
      auto& parent_ts = Scenario::parentTimeSync(*ev, scenar);
      if (parent_ts.events().size() > 1 || parent_ts.active())
        root.push_back(&parent_ts);
      else
        root.push_back(ev);
    }
    else if(auto ts = dynamic_cast<const Scenario::TimeSyncModel*>(sel))
    {
      root.push_back(ts);
    }
    else if(auto st = dynamic_cast<const Scenario::StateModel*>(sel))
    {
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*st);
      auto& parent_ev = Scenario::parentEvent(*st, scenar);
      if (parent_ev.states().size() > 1 || parent_ev.condition() != State::Expression{})
      {
        auto& parent_ts = Scenario::parentTimeSync(parent_ev, scenar);
        if (parent_ts.events().size() > 1 || parent_ts.active())
          root.push_back(&parent_ts);
        else
          root.push_back(&parent_ev);
      }
      else
        root.push_back(st);
    }
    else if(auto stp = dynamic_cast<const Process::StateProcess*>(sel))
    {
      auto state = static_cast<Scenario::StateModel*>(stp->parent());
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*state);
      root.push_back(&Scenario::parentTimeSync(*state, scenar));
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
  m_aliveMap.clear();

  for(auto obj : m_root)
  {
    m_aliveMap.insert(obj, obj);
    if(auto cst = dynamic_cast<const Scenario::IntervalModel*>(obj))
    {
      cst->processes.added.connect<ObjectItemModel, &ObjectItemModel::recompute>(*this);
      cst->processes.removed.connect<ObjectItemModel, &ObjectItemModel::recompute>(*this);

      for(auto& proc : cst->processes)
        m_aliveMap.insert(&proc, &proc);
    }
    else if (auto tn = dynamic_cast<const Scenario::TimeSyncModel*>(obj))
    {
      auto& scenar = Scenario::parentScenario(*tn);
      m_itemCon.push_back(connect(tn, &TimeSyncModel::newEvent, this, [=] { recompute(); }));
      m_itemCon.push_back(connect(tn, &TimeSyncModel::eventRemoved, this, [=] { recompute(); }));

      for(const auto& ev : tn->events())
      {
        auto& e = scenar.event(ev);
        m_aliveMap.insert(&e, &e);
        m_itemCon.push_back(con(e, &EventModel::statesChanged, this, [=] { recompute(); }));
        for(const auto& st : e.states())
        {
          auto& s = scenar.state(st);
          m_aliveMap.insert(&s, &s);
          s.stateProcesses.added.connect<ObjectItemModel, &ObjectItemModel::recompute>(*this);
          s.stateProcesses.removed.connect<ObjectItemModel, &ObjectItemModel::recompute>(*this);

          for(const auto& sp : s.stateProcesses)
            m_aliveMap.insert(&sp, &sp);
        }
      }
    }
    else if ( auto ev = dynamic_cast<const Scenario::EventModel*>(obj) )
    {
      auto& scenar = Scenario::parentScenario(*ev);
      auto& e = *ev;
      m_aliveMap.insert(&e, &e);
      m_itemCon.push_back(con(e, &EventModel::statesChanged, this, [=] { recompute(); }));
      for(const auto& st : e.states())
      {
        auto& s = scenar.state(st);
        m_aliveMap.insert(&s, &s);
        s.stateProcesses.added.connect<ObjectItemModel, &ObjectItemModel::recompute>(*this);
        s.stateProcesses.removed.connect<ObjectItemModel, &ObjectItemModel::recompute>(*this);

        for(const auto& sp : s.stateProcesses)
          m_aliveMap.insert(&sp, &sp);
      }
    }
    else if ( auto st = dynamic_cast<const Scenario::StateModel*>(obj) )
    {
      auto& s = *st;
      m_aliveMap.insert(&s, &s);
      s.stateProcesses.added.connect<ObjectItemModel, &ObjectItemModel::recompute>(*this);
      s.stateProcesses.removed.connect<ObjectItemModel, &ObjectItemModel::recompute>(*this);

      for(const auto& sp : s.stateProcesses)
        m_aliveMap.insert(&sp, &sp);
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
  if(isAlive(sel))
  {
    if(auto cst = dynamic_cast<Scenario::IntervalModel*>(sel))
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

      if(auto st = scenar.findState(*it))
        return createIndex(row, column, st);
    }
    else if(auto tn = dynamic_cast<Scenario::TimeSyncModel*>(sel))
    {
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*tn);
      auto it = tn->events().begin();
      std::advance(it, row);

      if(auto ev = scenar.findEvent(*it))
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
  return QModelIndex{};
}

bool ObjectItemModel::isAlive(QObject* obj) const
{
  if(!obj)
    return false;

  auto it = m_aliveMap.find(obj);
  if(it != m_aliveMap.end())
  {
    if(it->isNull())
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
  if(!isAlive(sel))
    return QModelIndex{};

  if(dynamic_cast<Scenario::IntervalModel*>(sel))
  {
    return QModelIndex{};
  }
  else if(auto ev = dynamic_cast<Scenario::EventModel*>(sel))
  {
    Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*ev);
    auto& tn = Scenario::parentTimeSync(*ev, scenar);
    auto idx = m_root.indexOf(&tn);
    if (idx >= 0)
      return createIndex(0, 0, (void*)m_root[idx]);
    else
      return QModelIndex{};
  }
  else if(dynamic_cast<Scenario::TimeSyncModel*>(sel))
  {
    return QModelIndex{};
  }
  else if(auto st = dynamic_cast<Scenario::StateModel*>(sel))
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
  else if(auto stp = dynamic_cast<Process::StateProcess*>(sel))
  {
    auto state = static_cast<Scenario::StateModel*>(stp->parent());
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
  else if(auto proc = dynamic_cast<Process::ProcessModel*>(sel))
  {
    auto idx = m_root.indexOf(proc->parent());
    if (idx >= 0)
      return createIndex(idx, 0, (void*)m_root[idx]);
    else
      return QModelIndex{};
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
  if(isAlive(sel))
  {
    if(auto cst = dynamic_cast<Scenario::IntervalModel*>(sel))
    {
      return cst->processes.size();
    }
    else if(auto ev = dynamic_cast<Scenario::EventModel*>(sel))
    {
      return ev->states().size();
    }
    else if(auto ts = dynamic_cast<Scenario::TimeSyncModel*>(sel))
    {
      return ts->events().size();
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
  if(!isAlive(sel))
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
      if(auto cst = dynamic_cast<Scenario::IntervalModel*>(sel))
      {
        return cst->metadata().getName();
      }
      else if(auto ev = dynamic_cast<Scenario::EventModel*>(sel))
      {
        return ev->metadata().getName();
      }
      else if(auto tn = dynamic_cast<Scenario::TimeSyncModel*>(sel))
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
      if(dynamic_cast<Scenario::IntervalModel*>(sel))
      {
        static const QIcon icon(":/images/interval.svg");
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
      else if(auto tn = dynamic_cast<Scenario::TimeSyncModel*>(sel))
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
      if(auto cst = dynamic_cast<Scenario::IntervalModel*>(sel))
      {
        return tr("Start : ") + cst->date().toString();
      }
      else if(auto ev = dynamic_cast<Scenario::EventModel*>(sel))
      {
        return ev->condition().toPrettyString();
      }
      else if(auto tn = dynamic_cast<Scenario::TimeSyncModel*>(sel))
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
  Qt::ItemFlags f = QAbstractItemModel::flags(index);
  f |= Qt::ItemIsSelectable;
  f |= Qt::ItemIsEnabled;
  f |= Qt::ItemIsEditable;
  return f;
}

bool ObjectItemModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if(index.column() == 0)
  {
    auto sel = (QObject*)index.internalPointer();
    if(!isAlive(sel))
      return {};

    CommandDispatcher<> disp{m_ctx.commandStack};

    if(auto cst = dynamic_cast<Scenario::IntervalModel*>(sel))
    {
    }
    else if(auto ev = dynamic_cast<Scenario::EventModel*>(sel))
    {
    }
    else if(auto tn = dynamic_cast<Scenario::TimeSyncModel*>(sel))
    {
    }
    else if(auto st = dynamic_cast<Scenario::StateModel*>(sel))
    {
    }
    else if(auto stp = dynamic_cast<Process::StateProcess*>(sel))
    {
    }
    else if(auto p = dynamic_cast<Process::ProcessModel*>(sel))
    {
      disp.submitCommand<Command::ChangeElementName<Process::ProcessModel>>(*p, value.toString());
      return true;
    }
  }
  return false;
}

SelectionStackWidget::SelectionStackWidget(
    score::SelectionStack& s, QWidget* parent, ObjectWidget* objects)
  : QWidget{parent}, m_stack{s}, m_selector{s, objects}
{
  m_prev = new QToolButton{this};
  m_prev->setArrowType(Qt::LeftArrow);
  m_prev->setEnabled(m_stack.canUnselect());

  m_label = new TextLabel{"History", this};

  m_next = new QToolButton{this};
  m_next->setArrowType(Qt::RightArrow);
  m_next->setEnabled(m_stack.canReselect());

  m_left = new QToolButton{this};
  m_left->setArrowType(Qt::LeftArrow);
  m_left->setEnabled(m_selector.hasLeft());

  m_right = new QToolButton{this};
  m_right->setArrowType(Qt::RightArrow);
  m_right->setEnabled(m_selector.hasRight());

  m_up = new QToolButton{this};
  m_up->setArrowType(Qt::UpArrow);
  m_up->setEnabled(m_selector.hasUp());

  m_down = new QToolButton{this};
  m_down->setArrowType(Qt::DownArrow);
  m_down->setEnabled(m_selector.hasDown());

  auto lay = new score::MarginLess<QHBoxLayout>{this};
  lay->setSizeConstraint(QLayout::SetMinimumSize);
  lay->addWidget(m_prev);
  lay->addWidget(m_label);
  lay->addWidget(m_next);
  QLabel* separator = new QLabel{"|"};
  lay->addWidget(separator);
  lay->addWidget(m_left);
  lay->addWidget(m_up);
  lay->addWidget(m_down);
  lay->addWidget(m_right);
  setLayout(lay);

  connect(m_prev, &QToolButton::pressed, [&]() { m_stack.unselect(); });
  connect(m_next, &QToolButton::pressed, [&]() { m_stack.reselect(); });

  connect(m_left, &QToolButton::pressed, [&]() { m_selector.selectLeft();});
  connect(m_right, &QToolButton::pressed, [&]() { m_selector.selectRight();});
  connect(m_up, &QToolButton::pressed, [&]() { m_selector.selectUp();});
  connect(m_down, &QToolButton::pressed, [&]() { m_selector.selectDown();});

  con(m_stack, &score::SelectionStack::currentSelectionChanged, this,
      [=] {
    m_prev->setEnabled(m_stack.canUnselect());
    m_next->setEnabled(m_stack.canReselect());
    m_left->setEnabled(m_selector.hasLeft());
    m_right->setEnabled(m_selector.hasRight());
    m_up->setEnabled(m_selector.hasUp());
    m_down->setEnabled(m_selector.hasDown());
  });
}

ObjectPanelDelegate::ObjectPanelDelegate(const score::GUIApplicationContext &ctx)
  : score::PanelDelegate{ctx}
  , m_widget{new SizePolicyWidget}
  , m_lay{new score::MarginLess<QVBoxLayout>{m_widget}}
  , m_searchWidget{new SearchWidget{ctx}}
{
  m_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  m_widget->setMinimumHeight(100);
  m_widget->setSizeHint({200, 100});
}

QWidget *ObjectPanelDelegate::widget()
{
  return m_widget;
}

const score::PanelStatus &ObjectPanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{true, Qt::RightDockWidgetArea, 12,
        QObject::tr("Objects"),
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

ObjectWidget::ObjectWidget(const score::DocumentContext& ctx, QWidget* par)
  : QTreeView{par}
  , model{ctx, this}
  , m_ctx{ctx}
{
  setModel(&model);
  setAnimated(true);
  setAlternatingRowColors(true);
  setMidLineWidth(40);
  setUniformRowHeights(true);
  setWordWrap(false);
  setMouseTracking(true);

  con(model, &ObjectItemModel::changed,
      this, &QTreeView::expandAll);
}

void ObjectWidget::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
  if((selected.size() > 0 || deselected.size() > 0) && !updatingSelection)
  {
    score::SelectionDispatcher d{m_ctx.selectionStack};
    auto sel = this->selectedIndexes();
    if(!sel.empty())
    {
      auto obj = (IdentifiedObjectAbstract*) sel.at(0).internalPointer();
      d.setAndCommit(Selection{obj});
    }
    else
    {
      d.setAndCommit({});
    }
  }
}

void ObjectWidget::contextMenuEvent(QContextMenuEvent* ev)
{
  auto point = ev->pos();
  auto index = indexAt(point);
  if (index.isValid())
  {
    auto ptr = (QObject*)index.internalPointer();
    if(!ptr)
      return;

    QMenu* m = new QMenu{this};

    if(auto cst = dynamic_cast<Scenario::IntervalModel*>(ptr))
    {
      auto addproc = new QAction{tr("Add process"), m};
      m->addAction(addproc);
      connect(addproc, &QAction::triggered, this, [=] {

        auto& fact = m_ctx.app.interfaces<Process::ProcessFactoryList>();
        auto dialog = new AddProcessDialog<Process::ProcessFactoryList>{fact, this};
        dialog->on_okPressed = [&] (const auto& proc) {
          using cmd = Scenario::Command::CreateProcessInNewSlot;
          QuietMacroCommandDispatcher<cmd> disp{m_ctx.commandStack};

          cmd::create(disp, *cst, proc);

          disp.commit();
        };

        dialog->launchWindow();
        dialog->deleteLater();
      });
    }
    else if(auto state = dynamic_cast<Scenario::StateModel*>(ptr))
    {
      auto addproc = new QAction{tr("Add process"), m};
      m->addAction(addproc);
      connect(addproc, &QAction::triggered, this, [=] {

        auto& fact = m_ctx.app.interfaces<Process::StateProcessList>();
        auto dialog = new AddProcessDialog<Process::StateProcessList>{fact, this};

        dialog->on_okPressed = [&] (const auto& proc) {
          CommandDispatcher<> disp{m_ctx.commandStack};
          disp.submitCommand<Scenario::Command::AddStateProcessToState>(*state, proc);
        };

        dialog->launchWindow();
        dialog->deleteLater();
      });
    }
    else if(auto proc = dynamic_cast<Process::ProcessModel*>(ptr))
    {
      auto deleteact = new QAction{tr("Remove"), m};
      m->addAction(deleteact);
      connect(deleteact, &QAction::triggered, this, [=] {
        CommandDispatcher<> c{m_ctx.commandStack};
        c.submitCommand<Scenario::Command::RemoveProcessFromInterval>(*(IntervalModel*)proc->parent(), proc->id());
      });
      auto duplicate = new QAction{tr("Duplicate"), m};
      m->addAction(duplicate);
      connect(duplicate, &QAction::triggered, this, [=] {
        CommandDispatcher<> c{m_ctx.commandStack};
        c.submitCommand<Scenario::Command::DuplicateOnlyProcessToInterval>(*(IntervalModel*)proc->parent(), *proc);
      });
    }
    else if(auto stp = dynamic_cast<Process::StateProcess*>(ptr))
    {
      auto deleteact = new QAction{tr("Remove"), m};
      m->addAction(deleteact);
      connect(deleteact, &QAction::triggered, this, [=] {
        CommandDispatcher<> c{m_ctx.commandStack};
        c.submitCommand<Scenario::Command::RemoveStateProcess>(*(StateModel*)stp->parent(), stp->id());
      });
    }

    m->exec(mapToGlobal(point));
    m->deleteLater();
  }
}

NeightborSelector::NeightborSelector(score::SelectionStack &s, ObjectWidget* objects)
  : m_stack{s}
  , m_objects{objects}
  , m_selectionDispatcher{s}
{

}
bool NeightborSelector::hasLeft() const
{
  for ( const auto& obj : m_stack.currentSelection() )
  {
    if (dynamic_cast<const IntervalModel*>(obj.data()))
    {
      // Interval always have previous state
      return true;
    }
    else if (auto state = dynamic_cast<const StateModel*>(obj.data()))
    {
      if (state->previousInterval())
        return true;
    }
  }
  return false;
}

bool NeightborSelector::hasRight() const
{
  for ( const auto& obj : m_stack.currentSelection() )
  {
    if (dynamic_cast<const IntervalModel*>(obj.data()))
    {
      // Interval always have previous state
      return true;
    }
    else if (auto state = dynamic_cast<const StateModel*>(obj.data()))
    {
      if (state->nextInterval())
        return true;
    }
  }
  return false;
}

bool NeightborSelector::hasUp() const
{
  auto cur_idx = m_objects->selectionModel()->currentIndex();
  auto idx = m_objects->indexAbove(cur_idx);
  bool res = idx.isValid();
  return res;
}

bool NeightborSelector::hasDown() const
{
  auto cur_idx = m_objects->selectionModel()->currentIndex();
  auto idx = m_objects->indexBelow(cur_idx);
  bool res = idx.isValid();
  return res;
}

void NeightborSelector::selectRight()
{
  Selection sel{};

  for ( const auto& obj : m_stack.currentSelection() )
  {
    if (auto interval = dynamic_cast<const IntervalModel*>(obj.data()))
    {
      // Interval always have previous state
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*interval);
      sel.append(&scenar.state(interval->endState()));
    }
    else if (auto state = dynamic_cast<const StateModel*>(obj.data()))
    {
      if (state->nextInterval())
      {
        Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*state);
        sel.append(&scenar.interval(*state->nextInterval()));
      }
    }
  }

  if (!sel.empty())
    m_selectionDispatcher.setAndCommit(sel);
}

void NeightborSelector::selectLeft()
{
  Selection sel{};

  for ( const auto& obj : m_stack.currentSelection() )
  {
    if (auto interval = dynamic_cast<const IntervalModel*>(obj.data()))
    {
      // Interval always have previous state
      Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*interval);
      sel.append(&scenar.state(interval->startState()));
    }
    else if (auto state = dynamic_cast<const StateModel*>(obj.data()))
    {
      if (state->previousInterval())
      {
        Scenario::ScenarioInterface& scenar = Scenario::parentScenario(*state);
        sel.append(&scenar.interval(*state->previousInterval()));
      }
    }
  }

  if (!sel.empty())
    m_selectionDispatcher.setAndCommit(sel);
}

void NeightborSelector::selectUp()
{
  auto cur_idx = m_objects->selectionModel()->currentIndex();
  auto idx = m_objects->indexAbove(cur_idx);
  if(idx.isValid())
  {
    Selection sel{};
    sel.append((IdentifiedObjectAbstract*) idx.internalPointer());
    m_selectionDispatcher.setAndCommit(sel);
  }
}

void NeightborSelector::selectDown()
{
  auto cur_idx = m_objects->selectionModel()->currentIndex();
  auto idx = m_objects->indexBelow(cur_idx);
  if(idx.isValid())
  {
    Selection sel{};
    sel.append((IdentifiedObjectAbstract*) idx.internalPointer());
    m_selectionDispatcher.setAndCommit(sel);
  }
}

SearchWidget::SearchWidget(const score::GUIApplicationContext& ctx)
  : m_ctx{ctx}
{
  setAcceptDrops(true);
  auto lay = new score::MarginLess<QHBoxLayout>{this};

  m_lineEdit = new QLineEdit();
  m_lineEdit->setPlaceholderText("Search");
  m_btn = new QPushButton();
  m_btn->setText("go");

  lay->addWidget(m_lineEdit);
  lay->addWidget(m_btn);
  setLayout(lay);

  connect(m_lineEdit, &QLineEdit::returnPressed, [&]() { search(); });
  connect(m_btn, &QPushButton::pressed, [&]() { search(); });

  const auto& appCtx = score::GUIAppContext();

  for (auto& cpt : appCtx.panels())
  {
    if (Explorer::PanelDelegate* panel = dynamic_cast<Explorer::PanelDelegate*>(&cpt))
    {
      Explorer::DeviceExplorerWidget* widget = static_cast<Explorer::DeviceExplorerWidget*>(panel->widget());
      connect(widget, &Explorer::DeviceExplorerWidget::findAddresses, this, &SearchWidget::on_findAddresses);
    }
  }
}

template<typename Object>
void add_if_contains(const Object& obj,const QString& str, Selection& sel)
{
  QJsonObject json = score::marshall<JSONObject>(obj);
  QJsonDocument doc{json};
  QString jstr{doc.toJson(QJsonDocument::Compact)};
  if (jstr.contains(str))
    sel.append(&obj);
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
  m_lineEdit->setText(searchTxt);
  search();
}

void SearchWidget::search()
{
  QString stxt = m_lineEdit->text();
  std::vector<State::AddressAccessor> addresses;

  int idx = stxt.indexOf("=");
  if (idx >= 0)
  {
    QString substr = stxt.mid(0,idx);
    if ( substr == "address")
    {
      QString addrstr = stxt.mid(idx+1);
      if (auto spaceidx = addrstr.indexOf(" ") >= 0)
        addrstr = substr.mid(0,spaceidx);

      int comma = addrstr.indexOf(",");
      int offset = 0;
      while (comma >= 0)
      {
        auto sub = addrstr.mid(offset,comma);
        auto optaddr = State::AddressAccessor::fromString(sub);
        if (optaddr)
          addresses.push_back(*optaddr);
        offset = comma+1;
        comma = addrstr.indexOf(",", offset);
      }
      auto sub = addrstr.mid(offset,comma);
      auto optaddr = State::AddressAccessor::fromString(sub);
      if (optaddr)
        addresses.push_back(*optaddr);
    }
  }

  if(addresses.empty())
  {
    auto opt = State::AddressAccessor::fromString(stxt);
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
      if (auto state = dynamic_cast<const StateModel*>(obj))
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
      else if (auto event = dynamic_cast<const EventModel*>(obj))
      {
        add_if_contains(*event, stxt, sel);
      }
      else if (auto ts = dynamic_cast<const TimeSyncModel*>(obj))
      {
        add_if_contains(*ts, stxt, sel);
      }
      else if (auto cmt = dynamic_cast<const CommentBlockModel*>(obj))
      {
        add_if_contains(*cmt, stxt, sel);
      }
      else if (auto interval = dynamic_cast<const IntervalModel*>(obj))
      {
        add_if_contains(*interval, stxt, sel);
      }
    }
  }

  score::SelectionDispatcher d{doc->context().selectionStack};
  d.setAndCommit(sel);
}

void SearchWidget::dragEnterEvent(QDragEnterEvent* event)
{
  const auto& formats = event->mimeData()->formats();
  if (formats.contains(score::mime::messagelist()) ||
      formats.contains(score::mime::addressettings()))
  {
    event->accept();
  }
}

void SearchWidget::dropEvent(QDropEvent* ev)
{
  auto& mime = *ev->mimeData();

  // TODO refactor this with AutomationPresenter and AddressLineEdit
  if (mime.formats().contains(score::mime::addressettings()))
  {
    Mime<Device::FullAddressSettings>::Deserializer des{mime};
    Device::FullAddressSettings as = des.deserialize();

    if (as.address.path.isEmpty())
      return;

    m_lineEdit->setText(as.address.toString());
    emit m_lineEdit->returnPressed();
  }
  else if (mime.formats().contains(score::mime::nodelist()))
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

      m_lineEdit->setText(as.address.toString());
      emit m_lineEdit->returnPressed();
    }
  }
  else if (mime.formats().contains(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    State::MessageList ml = des.deserialize();
    if (!ml.empty())
    {
      m_lineEdit->setText(ml[0].address.toString());
      emit m_lineEdit->returnPressed();
    }
  }
}
}

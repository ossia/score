// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioPresenter.hpp"

#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Application/Menus/ScenarioContextMenuManager.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Comment/SetCommentText.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateCommentBlock.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveCommentBlock.hpp>
#include <Scenario/Document/Interval/Graph/GraphIntervalPresenter.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Process/ScenarioView.hpp>
#include <State/MessageListSerialization.hpp>

#include <score/actions/ActionManager.hpp>

#include <QAction>
#include <QDebug>
#include <QMenu>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::ScenarioPresenter)
namespace Scenario
{
void updateTimeSyncExtent(TimeSyncPresenter& tn);

// Will call updateTimeSyncExtent
void updateEventExtent(ScenarioPresenter& pres, EventPresenter& ev, double view_height);

// Will call updateEventExtent
void updateIntervalVerticalPos(
    ScenarioPresenter& pres,
    IntervalModel& itv,
    double y,
    double view_height);

ScenarioPresenter::ScenarioPresenter(
    Scenario::EditionSettings& e,
    const Scenario::ProcessModel& scenario,
    Process::LayerView* view,
    const Process::Context& context,
    QObject* parent)
    : LayerPresenter{scenario, view, context, parent}
    , m_view{static_cast<ScenarioView*>(view)}
    , m_viewInterface{*this}
    , m_editionSettings{e}
    , m_ongoingDispatcher{context.commandStack}
    , m_selectionDispatcher{context.selectionStack}
    , m_sm{m_context, *this}
{
  m_view->init(this);
  /////// Setup of existing data
  // For each interval & event, display' em
  for (const auto& tn_model : scenario.timeSyncs)
  {
    on_timeSyncCreated(tn_model);
  }

  for (const auto& event_model : scenario.events)
  {
    on_eventCreated(event_model);
  }

  for (const auto& state_model : scenario.states)
  {
    on_stateCreated(state_model);
  }

  for (const auto& interval : scenario.intervals)
  {
    on_intervalCreated(interval);
  }

  for (const auto& cmt_model : scenario.comments)
  {
    on_commentCreated(cmt_model);
  }

  /////// Connections
  scenario.intervals.added.connect<&ScenarioPresenter::on_intervalCreated>(this);
  scenario.intervals.removed.connect<&ScenarioPresenter::on_intervalRemoved>(this);

  scenario.states.added.connect<&ScenarioPresenter::on_stateCreated>(this);
  scenario.states.removed.connect<&ScenarioPresenter::on_stateRemoved>(this);

  scenario.events.added.connect<&ScenarioPresenter::on_eventCreated>(this);
  scenario.events.removed.connect<&ScenarioPresenter::on_eventRemoved>(this);

  scenario.timeSyncs.added.connect<&ScenarioPresenter::on_timeSyncCreated>(this);
  scenario.timeSyncs.removed.connect<&ScenarioPresenter::on_timeSyncRemoved>(this);

  scenario.comments.added.connect<&ScenarioPresenter::on_commentCreated>(this);
  scenario.comments.removed.connect<&ScenarioPresenter::on_commentRemoved>(this);

  connect(m_view, &ScenarioView::keyPressed, this, [this](int k) {
    keyPressed(k);
    switch (k)
    {
      case Qt::Key_Left:
        return selectLeft();
      case Qt::Key_Right:
        return selectRight();
      case Qt::Key_Up:
        return selectUp();
      case Qt::Key_Down:
        return selectDown();
      default:
        break;
    }
  });
  connect(m_view, &ScenarioView::keyReleased, this, &ScenarioPresenter::keyReleased);

  connect(m_view, &ScenarioView::doubleClicked, this, &ScenarioPresenter::doubleClick);

  connect(m_view, &ScenarioView::askContextMenu, this, &ScenarioPresenter::contextMenuRequested);
  connect(m_view, &ScenarioView::dragEnter, this, [=](const QPointF& pos, const QMimeData& mime) {
    try
    {
      m_context.context.app.interfaces<Scenario::DropHandlerList>().dragEnter(*this, pos, mime);
    }
    catch (std::exception& e)
    {
      qDebug() << "Error during dragEnter: " << e.what();
    }
  });
  connect(m_view, &ScenarioView::dragMove, this, [=](const QPointF& pos, const QMimeData& mime) {
    try
    {
      m_context.context.app.interfaces<Scenario::DropHandlerList>().dragMove(*this, pos, mime);
    }
    catch (std::exception& e)
    {
      qDebug() << "Error during dragMove: " << e.what();
    }
  });
  connect(m_view, &ScenarioView::dragLeave, this, [=](const QPointF& pos, const QMimeData& mime) {
    try
    {
      stopDrawDragLine();
      m_context.context.app.interfaces<Scenario::DropHandlerList>().dragLeave(*this, pos, mime);
    }
    catch (std::exception& e)
    {
      qDebug() << "Error during dragLeave: " << e.what();
    }
  });
  connect(
      m_view, &ScenarioView::dropReceived, this, [=](const QPointF& pos, const QMimeData& mime) {
        try
        {
          stopDrawDragLine();
          m_context.context.app.interfaces<Scenario::DropHandlerList>().drop(*this, pos, mime);
        }
        catch (std::exception& e)
        {
          qDebug() << "Error during drop: " << e.what();
        }
      });

  m_graphicalScale = context.app.settings<Settings::Model>().getGraphicZoom();

  con(context.app.settings<Settings::Model>(),
      &Settings::Model::GraphicZoomChanged,
      this,
      [&](double d) {
        m_graphicalScale = d;
        m_viewInterface.on_graphicalScaleChanged(m_graphicalScale);
      });
  m_viewInterface.on_graphicalScaleChanged(m_graphicalScale);

  m_con = con(
      context.execTimer, &QTimer::timeout, this, &ScenarioPresenter::on_intervalExecutionTimer);

  auto& es = context.app.guiApplicationPlugin<ScenarioApplicationPlugin>().editionSettings();
  con(es, &EditionSettings::toolChanged, this, [=](Scenario::Tool t) {
    auto& skin = score::Skin::instance();
    switch (t)
    {
      case Scenario::Tool::Select:
        m_view->unsetCursor();
        break;
      case Scenario::Tool::Create:
      case Scenario::Tool::CreateGraph:
        m_view->setCursor(skin.CursorCreationMode);
        break;
      case Scenario::Tool::Play:
        m_view->setCursor(skin.CursorPlayFromHere);
        break;
      default:
        m_view->unsetCursor();
        break;
    }
  });
}

ScenarioPresenter::~ScenarioPresenter()
{
  disconnect(m_con);
  m_intervals.remove_all();
  m_states.remove_all();
  m_events.remove_all();
  m_timeSyncs.remove_all();
  m_comments.remove_all();
}

const Scenario::ProcessModel& ScenarioPresenter::model() const noexcept
{
  return static_cast<const Scenario::ProcessModel&>(m_process);
}

Point ScenarioPresenter::toScenarioPoint(QPointF pt) const noexcept
{
  return ConvertToScenarioPoint(pt, zoomRatio(), view().height());
}

QPointF ScenarioPresenter::fromScenarioPoint(const Scenario::Point& pt) const noexcept
{
  return ConvertFromScenarioPoint(pt, zoomRatio(), view().height());
}

void ScenarioPresenter::setWidth(qreal width, qreal defaultWidth)
{
  m_view->setWidth(width);
}

void ScenarioPresenter::setHeight(qreal height)
{
  m_view->setHeight(height);
  if (height > 10)
  {
    for (auto& ev : m_events)
    {
      updateEventExtent(*this, ev, height);
    }
  }
}

void ScenarioPresenter::putToFront()
{
  // m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, false);
  m_view->setOpacity(1);
}

void ScenarioPresenter::putBehind()
{
  // m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
  m_view->setOpacity(0.1);
}

void ScenarioPresenter::parentGeometryChanged()
{
  updateAllElements();
  m_view->update();
}

void ScenarioPresenter::on_zoomRatioChanged(ZoomRatio val)
{
  m_zoomRatio = val;
  if (val <= 0.)
    return;

  for (auto& interval : m_intervals)
  {
    interval.on_zoomRatioChanged(m_zoomRatio);
  }
  for (auto& interval : m_graphIntervals)
  {
    interval.resize();
  }
  for (auto& comment : m_comments)
  {
    comment.on_zoomRatioChanged(m_zoomRatio);
  }
}

TimeSyncPresenter& ScenarioPresenter::timeSync(const Id<TimeSyncModel>& id) const
{
  return m_timeSyncs.at(id);
}

IntervalPresenter& ScenarioPresenter::interval(const Id<IntervalModel>& id) const
{
  return m_intervals.at(id);
}

StatePresenter& ScenarioPresenter::state(const Id<StateModel>& id) const
{
  return m_states.at(id);
}

void ScenarioPresenter::setSnapLine(TimeVal t, bool enabled)
{
  if(enabled)
    view().setSnapLine(t.toPixels(m_zoomRatio));
  else
    view().setSnapLine(std::nullopt);
}

EventPresenter& ScenarioPresenter::event(const Id<EventModel>& id) const
{
  return m_events.at(id);
}

void ScenarioPresenter::fillContextMenu(
    QMenu& menu,
    QPoint pos,
    QPointF scenepos,
    const Process::LayerContextMenuManager& cm)
{
  auto& ctx = m_context.context;
  auto& actions = ctx.app.actions;

  // Get ScenarioModel actions
  cm.menu<ContextMenus::ScenarioModelContextMenu>().build(menu, pos, scenepos, this->context());
  menu.addSeparator();
  cm.menu<ContextMenus::IntervalContextMenu>().build(menu, pos, scenepos, this->context());

  menu.addSeparator();

  menu.addAction(actions.action<Actions::SelectAll>().action());
  menu.addAction(actions.action<Actions::DeselectAll>().action());

  auto createCommentAct = new QAction{"Add a Comment Block", &menu};
  connect(createCommentAct, &QAction::triggered, [&, scenepos]() {
    auto scenPoint = Scenario::ConvertToScenarioPoint(scenepos, zoomRatio(), view().height());

    auto cmd = new Scenario::Command::CreateCommentBlock{model(), scenPoint.date, scenPoint.y};
    CommandDispatcher<>{ctx.commandStack}.submit(cmd);
  });

  menu.addAction(createCommentAct);
}

void ScenarioPresenter::drawDragLine(const StateModel& st, Point pt) const
{
  auto& real_st = m_states.at(st.id());
  auto& ev = Scenario::parentEvent(model().states.at(st.id()), model());
  auto diff = pt.date - ev.date();
  m_view->drawDragLine(
      real_st.view()->pos(),
      {pt.date.toPixels(m_zoomRatio), pt.y * m_view->height()},
      diff.toString());
}

void ScenarioPresenter::stopDrawDragLine() const
{
  m_view->stopDrawDragLine();
}

template <typename Map, typename Id>
void ScenarioPresenter::removeElement(Map& map, const Id& id)
{
  map.erase(id);
  m_view->update();
}

void ScenarioPresenter::on_stateRemoved(const StateModel& state)
{
  EventPresenter& ev = m_events.at(state.eventId());
  ev.removeState(&m_states.at(state.id()));

  updateEventExtent(*this, ev, m_view->height());

#if defined(SCORE_DEBUG)
  for (auto& ev : m_events)
  {
    for (auto st : ev.states())
    {
      SCORE_ASSERT(st->id() != state.id());
    }
  }
#endif
  removeElement(m_states, state.id());
}

void ScenarioPresenter::on_eventRemoved(const EventModel& event)
{
  TimeSyncPresenter& ts = m_timeSyncs.at(event.timeSync());
  ts.removeEvent(&m_events.at(event.id()));
  updateTimeSyncExtent(ts);

  removeElement(m_events, event.id());
}

void ScenarioPresenter::on_timeSyncRemoved(const TimeSyncModel& timeSync)
{
  removeElement(m_timeSyncs, timeSync.id());
}

void ScenarioPresenter::on_intervalRemoved(const IntervalModel& cvm)
{
  if (Q_LIKELY(!cvm.graphal()))
    removeElement(m_intervals, cvm.id());
  else
    removeElement(m_graphIntervals, cvm.id());
}

void ScenarioPresenter::on_commentRemoved(const CommentBlockModel& cmt)
{
  removeElement(m_comments, cmt.id());
}

/////////////////////////////////////////////////////////////////////
// USER INTERACTIONS
void ScenarioPresenter::on_askUpdate()
{
  m_view->update();
}

void ScenarioPresenter::on_intervalExecutionTimer()
{
  // TODO optimize me by storing a list of the currently running intervals
  // TOOD loop

  for (TemporalIntervalPresenter& cst : m_intervals)
  {
    const auto& m = cst.model();
    if (!m.executing())
      continue;

    auto& v = *cst.view();
    const auto& dur = m.duration;

    const auto pp = dur.playPercentage();

    if (double w = cst.on_playPercentageChanged(pp))
    {
      const auto r = v.boundingRect();

      if (r.width() > 7.)
      {
        QRectF toUpdate = {r.x() + v.playWidth() - w, r.y(), 2. * w, 6.};
        if (!dur.isRigid())
        {
          double new_w = dur.isMaxInfinite() ? v.defaultWidth() - v.playWidth() + 2. * w
                                             : v.maxWidth() - v.playWidth() + 2. * w;
          toUpdate.setWidth(new_w);
        }
        v.update(toUpdate);
      }
      else if (pp == 0.)
      {
        v.update();
      }
    }
    else if (!dur.isRigid())
    {
      // We need to update in that case because
      // of the pulsing color of the interval
      const auto r = v.boundingRect();
      double new_w = dur.isMaxInfinite() ? v.defaultWidth() - v.minWidth() + 4.
                                         : v.maxWidth() - v.minWidth() + 4.;
      QRectF toUpdate = {r.x() + v.minWidth() - 2., r.y(), new_w, 6.};
      v.update(toUpdate);
    }
  }
}

void ScenarioPresenter::selectLeft()
{
  CategorisedScenario selection{this->model()};

  const auto n_itvs = selection.selectedIntervals.size();
  const auto n_ev = selection.selectedEvents.size();
  const auto n_states = selection.selectedStates.size();
  const auto n_syncs = selection.selectedTimeSyncs.size();
  switch (n_itvs + n_ev + n_states + n_syncs)
  {
    case 1:
    {
      if (n_itvs == 1)
      {
        auto& itv = *selection.selectedIntervals.front();
        auto& left_state = Scenario::startState(itv, model());
        score::SelectionDispatcher{m_context.context.selectionStack}.select(left_state);
      }
      else if (n_states == 1)
      {
        const Scenario::StateModel& st = *selection.selectedStates.front();
        if (st.previousInterval())
        {
          auto& left_itv = Scenario::previousInterval(st, model());
          score::SelectionDispatcher{m_context.context.selectionStack}.select(left_itv);
        }
      }
      break;
    }
    default:
      break;
  }
}

void ScenarioPresenter::selectRight()
{
  CategorisedScenario selection{this->model()};

  const auto n_itvs = selection.selectedIntervals.size();
  const auto n_ev = selection.selectedEvents.size();
  const auto n_states = selection.selectedStates.size();
  const auto n_syncs = selection.selectedTimeSyncs.size();
  switch (n_itvs + n_ev + n_states + n_syncs)
  {
    case 1:
    {
      if (n_itvs == 1)
      {
        auto& itv = *selection.selectedIntervals.front();
        auto& left_state = Scenario::endState(itv, model());
        score::SelectionDispatcher{m_context.context.selectionStack}.select(left_state);
      }
      else if (n_states == 1)
      {
        const Scenario::StateModel& st = *selection.selectedStates.front();
        if (st.nextInterval())
        {
          auto& left_itv = Scenario::nextInterval(st, model());
          score::SelectionDispatcher{m_context.context.selectionStack}.select(left_itv);
        }
      }
      break;
    }
    default:
      break;
  }
}

// TODO MOVEME
template <typename Scenario_T>
ossia::small_vector<StateModel*, 8> getStates(const TimeSyncModel& ts, const Scenario_T& scenario)
{
  ossia::small_vector<StateModel*, 8> states;
  states.reserve(ts.events().size() * 2);
  for (auto& ev : ts.events())
  {
    auto& e = scenario.event(ev);
    for (auto& state : e.states())
    {
      states.push_back(&scenario.state(state));
    }
  }
  return states;
}

void ScenarioPresenter::selectUp()
{
  CategorisedScenario selection{this->model()};

  const auto n_itvs = selection.selectedIntervals.size();
  const auto n_ev = selection.selectedEvents.size();
  const auto n_states = selection.selectedStates.size();
  const auto n_syncs = selection.selectedTimeSyncs.size();

  if (n_states == 1)
  {
    if (n_itvs || n_ev || n_syncs)
      return;

    const auto& sel_state = *selection.selectedStates.front();
    const auto& parent_ts = Scenario::parentTimeSync(sel_state, model());
    const auto states = Scenario::getStates(parent_ts, model());
    double min = -1.;
    const auto* cur_state = &sel_state;
    for (StateModel* state : states)
    {
      const auto h = state->heightPercentage();
      if (h < sel_state.heightPercentage() && h > min)
      {
        cur_state = state;
        min = h;
      }
    }

    if (cur_state != &sel_state)
    {
      score::SelectionDispatcher{m_context.context.selectionStack}.select(*cur_state);
    }
    else
    {
      score::SelectionDispatcher{m_context.context.selectionStack}.select(parent_ts);
    }
  }
  else
  {
    auto sel = m_context.context.selectionStack.currentSelection();
    if (sel.empty())
      return;
    for (auto& ptr : sel)
    {
      if (!ptr)
        continue;
      auto proc = qobject_cast<const Process::ProcessModel*>(ptr);
      if (!proc)
        continue;

      auto parent = ptr->parent();
      if (!parent)
        continue;
      if (auto gp = parent->parent(); gp != &model())
        continue;

      auto itv = qobject_cast<IntervalModel*>(parent);
      if (!itv)
        continue;

      const auto& rack = itv->smallView();
      std::size_t i = 0;
      for (const Slot& slot : rack)
      {
        if (ossia::contains(slot.processes, proc->id()))
        {
          if (i > 0) // at minimum zero since we're in a slot
          {
            if (const auto& prev_proc = rack[i - 1].frontProcess)
            {
              score::SelectionDispatcher{m_context.context.selectionStack}.select(
                  itv->processes.at(*prev_proc));
            }
          }
          else if (i == 0)
          {
            score::SelectionDispatcher{m_context.context.selectionStack}.select(*itv);
          }
        }
        i++;
      }
    }
  }
}

void ScenarioPresenter::selectDown()
{
  CategorisedScenario selection{this->model()};

  const auto n_itvs = selection.selectedIntervals.size();
  const auto n_ev = selection.selectedEvents.size();
  const auto n_states = selection.selectedStates.size();
  const auto n_syncs = selection.selectedTimeSyncs.size();

  if (n_syncs == 1)
  {
    // Select the topmost state
    if (n_itvs || n_ev || n_states)
      return;

    const auto& sel_sync = *selection.selectedTimeSyncs.front();
    const auto states = Scenario::getStates(sel_sync, model());
    if (states.empty())
      return;

    double max = std::numeric_limits<double>::max();
    const StateModel* cur_state = states.front();
    for (StateModel* state : states)
    {
      const auto h = state->heightPercentage();
      if (h < max)
      {
        cur_state = state;
        max = h;
      }
    }

    score::SelectionDispatcher{m_context.context.selectionStack}.select(*cur_state);
  }
  else if (n_itvs == 1)
  {
    // Select the process in the first slot, if any
    const IntervalModel& itv = *selection.selectedIntervals.front();
    if (itv.processes.empty())
      return;
    if (!itv.smallViewVisible())
      return;
    auto& slot = itv.getSmallViewSlot(0);
    auto& front = slot.frontProcess;
    if (front)
    {
      auto& proc = itv.processes.at(*front);
      score::SelectionDispatcher{m_context.context.selectionStack}.select(proc);
    }
  }
  else if (n_states == 1)
  {
    if (n_itvs || n_ev || n_syncs)
      return;

    const auto& sel_state = *selection.selectedStates.front();
    const auto& parent_ts = Scenario::parentTimeSync(sel_state, model());
    const auto states = Scenario::getStates(parent_ts, model());
    double max = std::numeric_limits<double>::max();
    const auto* cur_state = &sel_state;
    for (StateModel* state : states)
    {
      const auto h = state->heightPercentage();
      if (h > sel_state.heightPercentage() && h < max)
      {
        cur_state = state;
        max = h;
      }
    }

    if (cur_state != &sel_state)
      score::SelectionDispatcher{m_context.context.selectionStack}.select(*cur_state);
  }
  else
  {
    auto sel = m_context.context.selectionStack.currentSelection();
    if (sel.empty())
      return;
    for (auto& ptr : sel)
    {
      if (!ptr)
        continue;
      auto proc = qobject_cast<const Process::ProcessModel*>(ptr);
      if (!proc)
        continue;

      auto parent = ptr->parent();
      if (!parent)
        continue;
      if (auto gp = parent->parent(); gp != &model())
        continue;

      auto itv = qobject_cast<IntervalModel*>(parent);
      if (!itv)
        continue;

      const auto& rack = itv->smallView();
      std::size_t i = 0;
      for (const Slot& slot : rack)
      {
        if (ossia::contains(slot.processes, proc->id()))
        {
          if (i < rack.size() - 1) // at minimum zero since we're in a slot
          {
            if (const auto& next_proc = rack[i + 1].frontProcess)
            {
              score::SelectionDispatcher{m_context.context.selectionStack}.select(
                  itv->processes.at(*next_proc));
            }
          }
        }
        i++;
      }
    }
  }
}

void ScenarioPresenter::doubleClick(QPointF pt)
{
  if (m_editionSettings.tool() == Scenario::Tool::Play)
    return;

  auto sp = toScenarioPoint(pt);

  // Just create a dot
  auto cmd = new Command::CreateTimeSync_Event_State{model(), sp.date, sp.y};
  CommandDispatcher<>{m_context.context.commandStack}.submit(cmd);
}

void ScenarioPresenter::on_focusChanged()
{
  if (focused())
  {
    m_view->setFocus();
  }

  editionSettings().setTool(Scenario::Tool::Select);
}

/////////////////////////////////////////////////////////////////////
// ELEMENTS CREATED
void ScenarioPresenter::on_eventCreated(const EventModel& event_model)
{
  auto ev_pres = new EventPresenter{event_model, m_view, this};
  m_events.insert(ev_pres);

  TimeSyncPresenter& ts = m_timeSyncs.at(event_model.timeSync());
  ts.addEvent(ev_pres);
  ev_pres->view()->setWidthScale(m_graphicalScale);
  m_viewInterface.on_eventMoved(*ev_pres);

  con(*ev_pres, &EventPresenter::extentChanged, this, [=](const VerticalExtent&) {
    m_viewInterface.on_eventMoved(*ev_pres);
  });

  con(event_model, &EventModel::dateChanged, this, [=](const TimeVal&) {
    m_viewInterface.on_eventMoved(*ev_pres);
  });

  con(event_model,
      &EventModel::timeSyncChanged,
      this,
      [=](const Id<TimeSyncModel>& old_id, const Id<TimeSyncModel>& new_id) {
        auto& old_t = m_timeSyncs.at(old_id);
        old_t.removeEvent(ev_pres);
        auto& new_t = m_timeSyncs.at(new_id);
        new_t.addEvent(ev_pres);
      });

  // For the state machine
  connect(ev_pres, &EventPresenter::pressed, m_view, &ScenarioView::pressedAsked);
  connect(ev_pres, &EventPresenter::moved, m_view, &ScenarioView::movedAsked);
  connect(ev_pres, &EventPresenter::released, m_view, &ScenarioView::released);
}

void ScenarioPresenter::on_timeSyncCreated(const TimeSyncModel& timeSync_model)
{
  auto tn_pres = new TimeSyncPresenter{timeSync_model, m_view, this};
  m_timeSyncs.insert(tn_pres);

  m_viewInterface.on_timeSyncMoved(*tn_pres);

  con(*tn_pres, &TimeSyncPresenter::extentChanged, this, [=](const VerticalExtent&) {
    m_viewInterface.on_timeSyncMoved(*tn_pres);
  });
  con(timeSync_model, &TimeSyncModel::dateChanged, this, [=](const TimeVal&) {
    m_viewInterface.on_timeSyncMoved(*tn_pres);
  });

  // For the state machine
  connect(tn_pres, &TimeSyncPresenter::pressed, m_view, &ScenarioView::pressedAsked);
  connect(tn_pres, &TimeSyncPresenter::moved, m_view, &ScenarioView::movedAsked);
  connect(tn_pres, &TimeSyncPresenter::released, m_view, &ScenarioView::released);
}

void ScenarioPresenter::on_stateCreated(const StateModel& state)
{
  auto st_pres = new StatePresenter{state, m_context.context, m_view, this};
  m_states.insert(st_pres);

  st_pres->view()->setScale(m_graphicalScale);
  m_viewInterface.on_stateMoved(*st_pres);

  EventPresenter& ev_pres = m_events.at(state.eventId());
  /*
    for(auto& ev : m_events)
    {
      for(auto st : ev.states())
      {
        SCORE_ASSERT(st->id() != state.id());
      }
    }*/
  ev_pres.addState(st_pres);

  con(state, &StateModel::heightPercentageChanged, this, [this, st_pres] {
    m_viewInterface.on_stateMoved(*st_pres);
    updateEventExtent(*this, m_events.at(st_pres->model().eventId()), m_view->height());
  });
  con(state,
      &StateModel::eventChanged,
      this,
      [this, st_pres](const auto& oldid, const auto& newid) {
        EventPresenter& oldev = m_events.at(oldid);
        EventPresenter& newev = m_events.at(newid);
        const auto h = m_view->height();
        oldev.removeState(st_pres);
        newev.addState(st_pres);

        updateEventExtent(*this, oldev, h);
        updateEventExtent(*this, newev, h);
      });
  updateEventExtent(*this, ev_pres, m_view->height());

  // For the state machine
  connect(st_pres, &StatePresenter::pressed, m_view, &ScenarioView::pressedAsked);
  connect(st_pres, &StatePresenter::moved, m_view, &ScenarioView::movedAsked);
  connect(st_pres, &StatePresenter::released, m_view, &ScenarioView::released);
}

void ScenarioPresenter::on_intervalCreated(const IntervalModel& interval)
{
  auto& startEvent = Scenario::startEvent(interval, model());
  auto& endEvent = Scenario::endEvent(interval, model());
  auto& startEventPres = m_events.at(startEvent.id());
  auto& endEventPres = m_events.at(endEvent.id());
  if (Q_UNLIKELY(interval.graphal()))
  {
    auto& startState = Scenario::startState(interval, model());
    auto& endState = Scenario::endState(interval, model());
    auto& startStatePres = m_states.at(startState.id());
    auto& endStatePres = m_states.at(endState.id());

    auto cst_pres = new GraphalIntervalPresenter{
        interval, *startStatePres.view(), *endStatePres.view(), m_context.context, this->m_view};
    m_graphIntervals.insert(cst_pres);
    connect(cst_pres, &GraphalIntervalPresenter::pressed, m_view, &ScenarioView::pressedAsked);
    connect(cst_pres, &GraphalIntervalPresenter::moved, m_view, &ScenarioView::movedAsked);
    connect(cst_pres, &GraphalIntervalPresenter::released, m_view, &ScenarioView::released);

    con(startState,
        &StateModel::heightPercentageChanged,
        cst_pres,
        &GraphalIntervalPresenter::resize);
    con(endState,
        &StateModel::heightPercentageChanged,
        cst_pres,
        &GraphalIntervalPresenter::resize);
    con(startEvent, &EventModel::dateChanged, cst_pres, &GraphalIntervalPresenter::resize);
    con(endEvent, &EventModel::dateChanged, cst_pres, &GraphalIntervalPresenter::resize);

    // TODO are these two calls useful ?
    updateEventExtent(*this, startEventPres, m_view->height());
    updateEventExtent(*this, endEventPres, m_view->height());
  }
  else
  {
    auto cst_pres = new TemporalIntervalPresenter{interval, m_context.context, true, m_view, this};
    m_intervals.insert(cst_pres);
    cst_pres->on_zoomRatioChanged(m_zoomRatio);

    m_viewInterface.on_intervalMoved(*cst_pres);

    con(interval, &IntervalModel::requestHeightChange, this, [this, &interval](double y) {
      updateIntervalVerticalPos(*this, const_cast<IntervalModel&>(interval), y, m_view->height());
    });

    con(startEvent, &EventModel::statusChanged, cst_pres, [cst_pres] {
      cst_pres->view()->update();
    });
    con(endEvent, &EventModel::statusChanged, cst_pres, [cst_pres] {
      cst_pres->view()->update();
    });

    auto updateHeight = [this, &interval] {
      auto h = m_view->height();
      auto& startEvent = Scenario::startEvent(interval, model());
      auto& endEvent = Scenario::endEvent(interval, model());
      auto& startEventPres = m_events.at(startEvent.id());
      auto& endEventPres = m_events.at(endEvent.id());
      updateEventExtent(*this, startEventPres, h);
      updateEventExtent(*this, endEventPres, h);
    };
    updateEventExtent(*this, startEventPres, m_view->height());
    updateEventExtent(*this, endEventPres, m_view->height());
    con(interval, &IntervalModel::rackChanged, this, updateHeight);

    con(interval, &IntervalModel::smallViewVisibleChanged, this, updateHeight);

    con(interval, &IntervalModel::slotResized, this, updateHeight);

    con(interval, &IntervalModel::slotAdded, this, updateHeight);

    con(interval, &IntervalModel::slotRemoved, this, updateHeight);

    connect(cst_pres, &TemporalIntervalPresenter::heightPercentageChanged, this, [=]() {
      m_viewInterface.on_intervalMoved(*cst_pres);
    });
    con(interval, &IntervalModel::dateChanged, this, [=](const TimeVal&) {
      m_viewInterface.on_intervalMoved(*cst_pres);
    });
    connect(
        cst_pres, &TemporalIntervalPresenter::askUpdate, this, &ScenarioPresenter::on_askUpdate);

    // For the state machine
    connect(cst_pres, &TemporalIntervalPresenter::pressed, m_view, &ScenarioView::pressedAsked);
    connect(cst_pres, &TemporalIntervalPresenter::moved, m_view, &ScenarioView::movedAsked);
    connect(cst_pres, &TemporalIntervalPresenter::released, m_view, &ScenarioView::released);
  }
}

void ScenarioPresenter::on_commentCreated(const CommentBlockModel& comment_block_model)
{
  using namespace Scenario::Command;
  auto cmt_pres = new CommentBlockPresenter{comment_block_model, m_view, this};

  m_comments.insert(cmt_pres);
  m_viewInterface.on_commentMoved(*cmt_pres);

  con(comment_block_model, &CommentBlockModel::dateChanged, this, [=](const TimeVal&) {
    m_viewInterface.on_commentMoved(*cmt_pres);
  });
  con(comment_block_model, &CommentBlockModel::heightPercentageChanged, this, [=](double y) {
    m_viewInterface.on_commentMoved(*cmt_pres);
  });

  // Selection
  connect(cmt_pres, &CommentBlockPresenter::selected, this, [&]() {
    m_selectionDispatcher.select(comment_block_model);
  });

  // Commands
  connect(cmt_pres, &CommentBlockPresenter::moved, this, [&](QPointF scenPos) {
    auto pos = Scenario::ConvertToScenarioPoint(scenPos, m_zoomRatio, m_view->height());
    m_ongoingDispatcher.submit<MoveCommentBlock>(
        model(), comment_block_model.id(), pos.date, pos.y);
  });
  connect(cmt_pres, &CommentBlockPresenter::released, this, [&](QPointF scenPos) {
    m_ongoingDispatcher.commit();
  });

  connect(cmt_pres, &CommentBlockPresenter::editFinished, this, [&](const QString& doc) {
    if (focused() && doc != comment_block_model.content())
    {
      CommandDispatcher<> c{m_context.context.commandStack};
      c.submit(new SetCommentText{{comment_block_model}, doc});
    }
  });
}

void ScenarioPresenter::updateAllElements()
{
  for (auto& interval : m_intervals)
  {
    m_viewInterface.on_intervalMoved(interval);
  }

  for (auto& event : m_events)
  {
    m_viewInterface.on_eventMoved(event);
  }

  for (auto& timesync : m_timeSyncs)
  {
    m_viewInterface.on_timeSyncMoved(timesync);
  }

  for (auto& comment : m_comments)
  {
    m_viewInterface.on_commentMoved(comment);
  }

  for (auto& interval : m_graphIntervals)
  {
    interval.resize();
  }
}

const StateModel* furthestSelectedState(const Scenario::ProcessModel& scenar)
{
  const StateModel* furthest{};
  {
    TimeVal max_t = TimeVal::zero();
    double max_y = 0;
    for (StateModel& elt : scenar.states)
    {
      if (elt.selection.get())
      {
        auto date = scenar.events.at(elt.eventId()).date();
        if (!furthest || date > max_t)
        {
          max_t = date;
          max_y = elt.heightPercentage();
          furthest = &elt;
        }
        else if (date == max_t && elt.heightPercentage() > max_y)
        {
          max_y = elt.heightPercentage();
          furthest = &elt;
        }
      }
    }
    if (furthest)
    {
      return furthest;
    }
  }

  // If there is no furthest state, we instead go for a interval
  const IntervalModel* furthest_interval{};
  {
    TimeVal max_t = TimeVal::zero();
    double max_y = 0;
    for (IntervalModel& cst : scenar.intervals)
    {
      if (cst.selection.get())
      {
        auto date = cst.duration.defaultDuration();
        if (!furthest_interval || date > max_t)
        {
          max_t = date;
          max_y = cst.heightPercentage();
          furthest_interval = &cst;
        }
        else if (date == max_t && cst.heightPercentage() > max_y)
        {
          max_y = cst.heightPercentage();
          furthest_interval = &cst;
        }
      }
    }

    if (furthest_interval)
    {
      return &scenar.states.at(furthest_interval->endState());
    }
  }

  return nullptr;
}

const EventModel* furthestSelectedEvent(const Scenario::ScenarioPresenter& scenar)
{
  const EventModel* furthest{};
  {
    TimeVal max_t = TimeVal::zero();
    double max_y = 0;
    for (EventPresenter& ev : scenar.getEvents())
    {
      const EventModel& elt = ev.model();
      if (elt.selection.get())
      {
        const auto extent = ev.extent();
        auto date = elt.date();
        if (!furthest || date > max_t)
        {
          max_t = date;
          max_y = extent.bottom();
          furthest = &elt;
        }
        else if (date == max_t && extent.bottom() > max_y)
        {
          max_y = extent.bottom();
          furthest = &elt;
        }
      }
    }
  }
  return furthest;
}

const TimeSyncModel* furthestSelectedSync(const Scenario::ScenarioPresenter& scenar)
{
  const TimeSyncModel* furthest{};
  {
    TimeVal max_t = TimeVal::zero();
    double max_y = 0;
    for (TimeSyncPresenter& ts : scenar.getTimeSyncs())
    {
      const TimeSyncModel& elt = ts.model();
      if (elt.selection.get())
      {
        const auto extent = ts.extent();
        auto date = elt.date();
        if (!furthest || date > max_t)
        {
          max_t = date;
          max_y = extent.bottom();
          furthest = &elt;
        }
        else if (date == max_t && extent.bottom() > max_y)
        {
          max_y = extent.bottom();
          furthest = &elt;
        }
      }
    }
  }
  return furthest;
}

const StateModel*
furthestSelectedStateWithoutFollowingInterval(const Scenario::ProcessModel& scenar)
{
  const StateModel* furthest_state{};
  {
    TimeVal max_t = TimeVal::zero();
    double max_y = 0;
    const auto& sc_events = scenar.events;
    for (StateModel& state : scenar.states)
    {
      if (state.selection.get() && !state.nextInterval())
      {
        auto date = sc_events.at(state.eventId()).date();
        if (!furthest_state || date > max_t)
        {
          max_t = date;
          max_y = state.heightPercentage();
          furthest_state = &state;
        }
        else if (date == max_t && state.heightPercentage() > max_y)
        {
          max_y = state.heightPercentage();
          furthest_state = &state;
        }
      }
    }
    if (furthest_state)
    {
      return furthest_state;
    }
  }

  // If there is no furthest state, we instead go for a interval
  const IntervalModel* furthest_interval{};
  {
    TimeVal max_t = TimeVal::zero();
    double max_y = 0;
    for (IntervalModel& cst : scenar.intervals)
    {
      if (cst.selection.get())
      {
        const auto& state = scenar.states.at(cst.endState());
        if (state.nextInterval())
          continue;

        auto date = cst.duration.defaultDuration();
        if (!furthest_interval || date > max_t)
        {
          max_t = date;
          max_y = cst.heightPercentage();
          furthest_interval = &cst;
        }
        else if (date == max_t && cst.heightPercentage() > max_y)
        {
          max_y = cst.heightPercentage();
          furthest_interval = &cst;
        }
      }
    }

    if (furthest_interval)
    {
      return &scenar.states.at(furthest_interval->endState());
    }
  }

  return nullptr;
}

const TimeSyncModel* furthestHierarchicallySelectedTimeSync(const ScenarioPresenter& scenario)
{
  const Scenario::TimeSyncModel* attach_sync{};
  auto& model = scenario.model();

  if (auto furthestState = furthestSelectedState(model))
  {
    attach_sync = &Scenario::parentTimeSync(*furthestState, model);
  }
  else
  {
    if (auto furthestEvent = furthestSelectedEvent(scenario))
    {
      attach_sync = &Scenario::parentTimeSync(*furthestEvent, model);
    }
    else
    {
      attach_sync = furthestSelectedSync(scenario);
    }
  }

  if (!attach_sync)
    attach_sync = &model.startTimeSync();

  return attach_sync;
}
}

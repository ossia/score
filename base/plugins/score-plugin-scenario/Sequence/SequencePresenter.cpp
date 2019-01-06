// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SequencePresenter.hpp"

#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Application/Menus/ScenarioContextMenuManager.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Comment/SetCommentText.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateCommentBlock.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveCommentBlock.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Sequence/SequenceView.hpp>
#include <State/MessageListSerialization.hpp>

#include <score/actions/ActionManager.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Sequence::SequencePresenter)
namespace Sequence
{
SequencePresenter::SequencePresenter(
    Scenario::EditionSettings& e, const Sequence::ProcessModel& scenario,
    Process::LayerView* view, const Process::ProcessPresenterContext& context,
    QObject* parent)
    : LayerPresenter{context, parent}
    , m_layer{scenario}
    , m_view{static_cast<SequenceView*>(view)}
    , m_viewInterface{*this}
    , m_editionSettings{e}
    , m_selectionDispatcher{context.selectionStack}
    //, m_sm{m_context, *this}
{
  /////// Setup of existing data
  // For each interval & event, display' em
  for (const auto& state_model : scenario.states)
  {
    on_stateCreated(state_model);
  }

  for (const auto& event_model : scenario.events)
  {
    on_eventCreated(event_model);
  }

  for (const auto& tn_model : scenario.timeSyncs)
  {
    on_timeSyncCreated(tn_model);
  }

  for (const auto& interval : scenario.intervals)
  {
    on_intervalCreated(interval);
  }

  /////// Connections
  scenario.intervals.added
      .connect<&SequencePresenter::on_intervalCreated>(this);
  scenario.intervals.removed
      .connect<&SequencePresenter::on_intervalRemoved>(this);

  scenario.states.added.connect<&SequencePresenter::on_stateCreated>(
      this);
  scenario.states.removed.connect<&SequencePresenter::on_stateRemoved>(
      this);

  scenario.events.added.connect<&SequencePresenter::on_eventCreated>(
      this);
  scenario.events.removed.connect<&SequencePresenter::on_eventRemoved>(
      this);

  scenario.timeSyncs.added
      .connect<&SequencePresenter::on_timeSyncCreated>(this);
  scenario.timeSyncs.removed
      .connect<&SequencePresenter::on_timeSyncRemoved>(this);

  connect(
      m_view, &SequenceView::askContextMenu, this,
      &SequencePresenter::contextMenuRequested);

  m_graphicalScale = context.app.settings<Scenario::Settings::Model>().getGraphicZoom();

  con(context.app.settings<Scenario::Settings::Model>(),
      &Scenario::Settings::Model::GraphicZoomChanged, this, [&](double d) {
        m_graphicalScale = d;
        m_viewInterface.on_graphicalScaleChanged(m_graphicalScale);
      });
  m_viewInterface.on_graphicalScaleChanged(m_graphicalScale);

  m_con = con(
      context.execTimer, &QTimer::timeout, this,
      &SequencePresenter::on_intervalExecutionTimer);
}

SequencePresenter::~SequencePresenter()
{
  disconnect(m_con);
  m_intervals.remove_all();
  m_states.remove_all();
  m_events.remove_all();
  m_timeSyncs.remove_all();
}

const Sequence::ProcessModel& SequencePresenter::model() const
{
  return m_layer;
}

const Id<Process::ProcessModel>& SequencePresenter::modelId() const
{
  return m_layer.id();
}

Scenario::Point SequencePresenter::toScenarioPoint(QPointF pt) const
{
  return Scenario::ConvertToScenarioPoint(pt, zoomRatio(), view().height());
}

void SequencePresenter::setWidth(qreal width)
{
  m_view->setWidth(width);
}

void SequencePresenter::setHeight(qreal height)
{
  m_view->setHeight(height);
}

void SequencePresenter::putToFront()
{
  m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, false);
  m_view->setOpacity(1);
}

void SequencePresenter::putBehind()
{
  m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
  m_view->setOpacity(0.1);
}

void SequencePresenter::parentGeometryChanged()
{
  updateAllElements();
  m_view->update();
}

void SequencePresenter::on_zoomRatioChanged(ZoomRatio val)
{
  m_zoomRatio = val;

  for (auto& interval : m_intervals)
  {
    interval.on_zoomRatioChanged(m_zoomRatio);
  }
}

TimeSyncPresenter&
SequencePresenter::timeSync(const Id<Scenario::TimeSyncModel>& id) const
{
  return m_timeSyncs.at(id);
}

IntervalPresenter&
SequencePresenter::interval(const Id<IntervalModel>& id) const
{
  return m_intervals.at(id);
}

StatePresenter&
SequencePresenter::state(const Id<Scenario::StateModel>& id) const
{
  return m_states.at(id);
}

EventPresenter&
SequencePresenter::event(const Id<Scenario::EventModel>& id) const
{
  return m_events.at(id);
}

void SequencePresenter::fillContextMenu(
    QMenu& menu, QPoint pos, QPointF scenepos,
    const Process::LayerContextMenuManager& cm)
{
  auto& ctx = m_context.context;
  auto& actions = ctx.app.actions;

  // Get SequenceModel actions
  cm.menu<ContextMenus::ScenarioModelContextMenu>().build(
      menu, pos, scenepos, this->context());
  menu.addSeparator();
  cm.menu<ContextMenus::IntervalContextMenu>().build(
      menu, pos, scenepos, this->context());
  cm.menu<ContextMenus::EventContextMenu>().build(
      menu, pos, scenepos, this->context());
  cm.menu<ContextMenus::StateContextMenu>().build(
      menu, pos, scenepos, this->context());

  menu.addSeparator();

  menu.addAction(actions.action<Actions::SelectAll>().action());
  menu.addAction(actions.action<Actions::DeselectAll>().action());
}

template <typename Map, typename Id>
void SequencePresenter::removeElement(Map& map, const Id& id)
{
  map.erase(id);
  m_view->update();
}

void SequencePresenter::on_stateRemoved(const StateModel& state)
{
  removeElement(m_states, state.id());
}

void SequencePresenter::on_eventRemoved(const EventModel& event)
{
  removeElement(m_events, event.id());
}

void SequencePresenter::on_timeSyncRemoved(
    const TimeSyncModel& timeSync)
{
  removeElement(m_timeSyncs, timeSync.id());
}

void SequencePresenter::on_intervalRemoved(const IntervalModel& cvm)
{
  removeElement(m_intervals, cvm.id());
}

/////////////////////////////////////////////////////////////////////
// USER INTERACTIONS
void SequencePresenter::on_askUpdate()
{
  m_view->update();
}

void SequencePresenter::on_intervalExecutionTimer()
{
  for (TemporalIntervalPresenter& cst : m_intervals)
  {
    auto pp = cst.model().duration.playPercentage();

    if (double w = cst.on_playPercentageChanged(pp))
    {
      auto& v = *cst.view();
      const auto r = v.boundingRect();

      if (r.width() > 7.)
        v.update(r.x() + v.playWidth() - w, r.y(), 2 * w, 5.);
      else if (pp == 0.)
        v.update();
    }
  }
}

void SequencePresenter::on_focusChanged()
{
  if (focused())
  {
    m_view->setFocus();
  }

  //editionSettings().setTool(Scenario::Tool::Select);
  //editionSettings().setExpandMode(ExpandMode::Scale);
}

/////////////////////////////////////////////////////////////////////
// ELEMENTS CREATED
void SequencePresenter::on_eventCreated(const EventModel& event_model)
{
  auto ev_pres = new EventPresenter{event_model, m_view, this};
  m_events.insert(ev_pres);

  ev_pres->view()->setWidthScale(m_graphicalScale);
  m_viewInterface.on_eventMoved(*ev_pres);

  con(event_model, &EventModel::extentChanged, this,
      [=](const Scenario::VerticalExtent&) { m_viewInterface.on_eventMoved(*ev_pres); });
  con(event_model, &EventModel::dateChanged, this,
      [=](const TimeVal&) { m_viewInterface.on_eventMoved(*ev_pres); });

  connect(ev_pres, &EventPresenter::eventHoverEnter, this, [=]() {
    m_viewInterface.on_hoverOnEvent(ev_pres->id(), true);
  });
  connect(ev_pres, &EventPresenter::eventHoverLeave, this, [=]() {
    m_viewInterface.on_hoverOnEvent(ev_pres->id(), false);
  });

  // For the state machine
  connect(
      ev_pres, &EventPresenter::pressed, m_view,
      &SequenceView::pressedAsked);
  connect(
      ev_pres, &EventPresenter::moved, m_view,
      &SequenceView::movedAsked);
  connect(
      ev_pres, &EventPresenter::released, m_view,
      &SequenceView::released);
}

void SequencePresenter::on_timeSyncCreated(
    const TimeSyncModel& timeSync_model)
{
  auto tn_pres = new TimeSyncPresenter{timeSync_model, m_view, this};
  m_timeSyncs.insert(tn_pres);

  m_viewInterface.on_timeSyncMoved(*tn_pres);

  con(timeSync_model, &TimeSyncModel::extentChanged, this,
      [=](const Scenario::VerticalExtent&) {
        m_viewInterface.on_timeSyncMoved(*tn_pres);
      });
  con(timeSync_model, &TimeSyncModel::dateChanged, this,
      [=](const TimeVal&) { m_viewInterface.on_timeSyncMoved(*tn_pres); });

  // For the state machine
  connect(
      tn_pres, &TimeSyncPresenter::pressed, m_view,
      &SequenceView::pressedAsked);
  connect(
      tn_pres, &TimeSyncPresenter::moved, m_view,
      &SequenceView::movedAsked);
  connect(
      tn_pres, &TimeSyncPresenter::released, m_view,
      &SequenceView::released);
}

void SequencePresenter::on_stateCreated(const StateModel& state)
{
  auto st_pres = new StatePresenter{state, m_context.context, m_view, this};
  m_states.insert(st_pres);

  st_pres->view()->setScale(m_graphicalScale);
  m_viewInterface.on_stateMoved(*st_pres);

  con(state, &StateModel::heightPercentageChanged, this,
      [=]() { m_viewInterface.on_stateMoved(*st_pres); });

  // For the state machine
  connect(
      st_pres, &StatePresenter::pressed, m_view,
      &SequenceView::pressedAsked);
  connect(
      st_pres, &StatePresenter::moved, m_view,
      &SequenceView::movedAsked);
  connect(
      st_pres, &StatePresenter::released, m_view,
      &SequenceView::released);

  connect(
      st_pres, &StatePresenter::askUpdate, this,
      &SequencePresenter::on_askUpdate);
}

void SequencePresenter::on_intervalCreated(
    const IntervalModel& interval)
{
  auto cst_pres = new TemporalIntervalPresenter{interval, m_context.context,
                                                true, m_view, this};
  m_intervals.insert(cst_pres);
  cst_pres->on_zoomRatioChanged(m_zoomRatio);

  m_viewInterface.on_intervalMoved(*cst_pres);

  connect(
      cst_pres, &TemporalIntervalPresenter::heightPercentageChanged, this,
      [=]() { m_viewInterface.on_intervalMoved(*cst_pres); });
  con(interval, &IntervalModel::dateChanged, this,
      [=](const TimeVal&) { m_viewInterface.on_intervalMoved(*cst_pres); });
  connect(
      cst_pres, &TemporalIntervalPresenter::askUpdate, this,
      &SequencePresenter::on_askUpdate);

  connect(cst_pres, &TemporalIntervalPresenter::intervalHoverEnter, [=]() {
    m_viewInterface.on_hoverOnInterval(cst_pres->model().id(), true);
  });
  connect(cst_pres, &TemporalIntervalPresenter::intervalHoverLeave, [=]() {
    m_viewInterface.on_hoverOnInterval(cst_pres->model().id(), false);
  });

  // For the state machine
  connect(
      cst_pres, &TemporalIntervalPresenter::pressed, m_view,
      &SequenceView::pressedAsked);
  connect(
      cst_pres, &TemporalIntervalPresenter::moved, m_view,
      &SequenceView::movedAsked);
  connect(
      cst_pres, &TemporalIntervalPresenter::released, m_view,
      &SequenceView::released);
}

void SequencePresenter::updateAllElements()
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
}







ViewUpdater::ViewUpdater(
    const SequencePresenter& presenter)
    : m_presenter{presenter}
{
}

void ViewUpdater::on_eventMoved(const EventPresenter& ev)
{
  auto h = m_presenter.m_view->boundingRect().height();

  ev.view()->setExtent(ev.model().extent() * h);

  ev.view()->setPos({ev.model().date().toPixels(m_presenter.m_zoomRatio),
                     ev.model().extent().top() * h});

  // We also have to move all the relevant states
  for (const auto& state : ev.model().states())
  {
    auto state_it = m_presenter.m_states.find(state);
    if (state_it != m_presenter.m_states.end())
    {
      on_stateMoved(*state_it);
    }
  }
  m_presenter.m_view->update();
}

void ViewUpdater::on_intervalMoved(
    const TemporalIntervalPresenter& pres)
{
  auto rect = m_presenter.m_view->boundingRect();
  auto msPerPixel = m_presenter.m_zoomRatio;

  const auto& cstr_model = pres.model();
  auto& cstr_view = view(pres);

  double startPos = cstr_model.date().toPixels(msPerPixel);
  // double delta = cstr_view.x() - startPos;
  bool dateChanged = true; // Disabled because it does a whacky movement when
                           // there are processes. (delta * delta > 1); //
                           // Magnetism

  if (dateChanged)
  {
    cstr_view.setPos(startPos, rect.height() * cstr_model.heightPercentage());
  }
  else
  {
    cstr_view.setY(qreal(rect.height() * cstr_model.heightPercentage()));
  }

  cstr_view.setDefaultWidth(
      cstr_model.duration.defaultDuration().toPixels(msPerPixel));
  cstr_view.setMinWidth(
      cstr_model.duration.minDuration().toPixels(msPerPixel));
  cstr_view.setMaxWidth(
      cstr_model.duration.maxDuration().isInfinite(),
      cstr_model.duration.maxDuration().isInfinite()
          ? -1
          : cstr_model.duration.maxDuration().toPixels(msPerPixel));

  m_presenter.m_view->update();
}

void ViewUpdater::on_timeSyncMoved(const TimeSyncPresenter& timesync)
{
  auto h = m_presenter.m_view->boundingRect().height();
  timesync.view()->setExtent(timesync.model().extent() * h);

  timesync.view()->setPos(
      {timesync.model().date().toPixels(m_presenter.m_zoomRatio),
       timesync.model().extent().top() * h});

  m_presenter.m_view->update();
}

void ViewUpdater::on_stateMoved(const StatePresenter& state)
{
  auto rect = m_presenter.m_view->boundingRect();
  const auto& ev = m_presenter.model().event(state.model().eventId());

  state.view()->setPos({ev.date().toPixels(m_presenter.m_zoomRatio),
                        rect.height() * state.model().heightPercentage()});

  m_presenter.m_view->update();
}

template <typename T>
void update_min_max(const T& val, T& min, T& max)
{
  min = val < min ? val : min;
  max = val > max ? val : max;
}

void ViewUpdater::on_hoverOnInterval(
    const Id<IntervalModel>& intervalId, bool enter)
{
}

void ViewUpdater::on_hoverOnEvent(
    const Id<EventModel>& eventId, bool enter)
{
}

void ViewUpdater::on_graphicalScaleChanged(double scale)
{
  for (auto& e : m_presenter.getEvents())
  {
    e.view()->setWidthScale(scale);
  }
  for (auto& s : m_presenter.getStates())
  {
    s.view()->setScale(scale);
  }

  m_presenter.m_view->update();
}
}

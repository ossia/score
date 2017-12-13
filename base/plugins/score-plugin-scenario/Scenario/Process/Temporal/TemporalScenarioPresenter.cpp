// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Comment/SetCommentText.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateCommentBlock.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveCommentBlock.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <State/MessageListSerialization.hpp>

#include "TemporalScenarioPresenter.hpp"
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Application/Menus/ScenarioContextMenuManager.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <score/actions/ActionManager.hpp>

class MessageItemModel;
class QMenu;

namespace Scenario
{
struct VerticalExtent;

TemporalScenarioPresenter::TemporalScenarioPresenter(
    Scenario::EditionSettings& e,
    const Scenario::ProcessModel& scenario,
    Process::LayerView* view,
    const Process::ProcessPresenterContext& context,
    QObject* parent)
  : LayerPresenter{context, parent}
  , m_layer{scenario}
  , m_view{static_cast<TemporalScenarioView*>(view)}
  , m_viewInterface{*this}
  , m_editionSettings{e}
  , m_ongoingDispatcher{context.commandStack}
  , m_selectionDispatcher{context.selectionStack}
  , m_sm{m_context, *this}
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

  for (const auto& cmt_model : scenario.comments)
  {
    on_commentCreated(cmt_model);
  }

  for (const auto& interval : scenario.intervals)
  {
    on_intervalCreated(interval);
  }

  /////// Connections
  scenario.intervals.added
      .connect<TemporalScenarioPresenter, &TemporalScenarioPresenter::on_intervalCreated>(
        this);
  scenario.intervals.removed
      .connect<TemporalScenarioPresenter, &TemporalScenarioPresenter::on_intervalRemoved>(
        this);

  scenario.states.added
      .connect<TemporalScenarioPresenter, &TemporalScenarioPresenter::on_stateCreated>(
        this);
  scenario.states.removed
      .connect<TemporalScenarioPresenter, &TemporalScenarioPresenter::on_stateRemoved>(
        this);

  scenario.events.added
      .connect<TemporalScenarioPresenter, &TemporalScenarioPresenter::on_eventCreated>(
        this);
  scenario.events.removed
      .connect<TemporalScenarioPresenter, &TemporalScenarioPresenter::on_eventRemoved>(
        this);

  scenario.timeSyncs.added
      .connect<TemporalScenarioPresenter, &TemporalScenarioPresenter::on_timeSyncCreated>(
        this);
  scenario.timeSyncs.removed
      .connect<TemporalScenarioPresenter, &TemporalScenarioPresenter::on_timeSyncRemoved>(
        this);

  scenario.comments.added
      .connect<TemporalScenarioPresenter, &TemporalScenarioPresenter::on_commentCreated>(
        this);
  scenario.comments.removed
      .connect<TemporalScenarioPresenter, &TemporalScenarioPresenter::on_commentRemoved>(
        this);


  connect(
        m_view, &TemporalScenarioView::keyPressed, this,
        &TemporalScenarioPresenter::on_keyPressed);
  connect(
        m_view, &TemporalScenarioView::keyReleased, this,
        &TemporalScenarioPresenter::on_keyReleased);

  connect(
        m_view, &TemporalScenarioView::doubleClick, this,
        &TemporalScenarioPresenter::doubleClick);

  connect(
        m_view, &TemporalScenarioView::askContextMenu, this,
        &TemporalScenarioPresenter::contextMenuRequested);
  connect(
        m_view, &TemporalScenarioView::dragEnter, this,
        [=](const QPointF& pos, const QMimeData* mime) {
    try
    {
      m_context.context.app.interfaces<Scenario::DropHandlerList>()
          .dragEnter(*this, pos, mime);
    }
    catch (std::exception& e)
    {
      qDebug() << "Error during dragEnter: " << e.what();
    }
  });
  connect(
        m_view, &TemporalScenarioView::dragMove, this,
        [=](const QPointF& pos, const QMimeData* mime) {
    try
    {
      m_context.context.app.interfaces<Scenario::DropHandlerList>()
          .dragMove(*this, pos, mime);
    }
    catch (std::exception& e)
    {
      qDebug() << "Error during dragMove: " << e.what();
    }
  });
  connect(
        m_view, &TemporalScenarioView::dragLeave, this,
        [=](const QPointF& pos, const QMimeData* mime) {
    try
    {
      m_context.context.app.interfaces<Scenario::DropHandlerList>()
          .dragLeave(*this, pos, mime);
    }
    catch (std::exception& e)
    {
      qDebug() << "Error during dragLeave: " << e.what();
    }
  });
  connect(
        m_view, &TemporalScenarioView::dropReceived, this,
        [=](const QPointF& pos, const QMimeData* mime) {
    try
    {
      m_context.context.app.interfaces<Scenario::DropHandlerList>()
          .drop(*this, pos, mime);
    }
    catch (std::exception& e)
    {
      qDebug() << "Error during drop: " << e.what();
    }
  });

  m_graphicalScale = context.app.settings<Settings::Model>().getGraphicZoom();

  con(context.app.settings<Settings::Model>(),
      &Settings::Model::GraphicZoomChanged, this, [&](double d) {
    m_graphicalScale = d;
    m_viewInterface.on_graphicalScaleChanged(m_graphicalScale);
  });
  m_viewInterface.on_graphicalScaleChanged(m_graphicalScale);

  m_con = con(
        context.execTimer, &QTimer::timeout, this,
        &TemporalScenarioPresenter::on_intervalExecutionTimer);

  auto& es = context.app.guiApplicationPlugin<ScenarioApplicationPlugin>()
                 .editionSettings();
  con(es, &EditionSettings::toolChanged, this, [=] (Scenario::Tool t) {
    switch (t)
    {
      case Scenario::Tool::Select:
        m_view->unsetCursor();
        break;
      case Scenario::Tool::Create:
        m_view->setCursor(QCursor(Qt::CrossCursor));
        break;
      case Scenario::Tool::Play:
        m_view->setCursor(QCursor(Qt::PointingHandCursor));
        break;
      default:
        m_view->unsetCursor();
        break;
    }
  });
}

TemporalScenarioPresenter::~TemporalScenarioPresenter()
{
  disconnect(m_con);
}

const Scenario::ProcessModel& TemporalScenarioPresenter::model() const
{
  return m_layer;
}

const Id<Process::ProcessModel>& TemporalScenarioPresenter::modelId() const
{
  return m_layer.id();
}

Point TemporalScenarioPresenter::toScenarioPoint(QPointF pt) const
{
  return ConvertToScenarioPoint(pt, zoomRatio(), view().height());
}

void TemporalScenarioPresenter::setWidth(qreal width)
{
  m_view->setWidth(width);
}

void TemporalScenarioPresenter::setHeight(qreal height)
{
  m_view->setHeight(height);
}

void TemporalScenarioPresenter::putToFront()
{
  m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, false);
  m_view->setOpacity(1);
}

void TemporalScenarioPresenter::putBehind()
{
  m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
  m_view->setOpacity(0.1);
}

void TemporalScenarioPresenter::parentGeometryChanged()
{
  updateAllElements();
  m_view->update();
}

void TemporalScenarioPresenter::on_zoomRatioChanged(ZoomRatio val)
{
  m_zoomRatio = val;

  for (auto& interval : m_intervals)
  {
    interval.on_zoomRatioChanged(m_zoomRatio);
  }
  for (auto& comment : m_comments)
  {
    comment.on_zoomRatioChanged(m_zoomRatio);
  }
}

void TemporalScenarioPresenter::fillContextMenu(
    QMenu& menu,
    QPoint pos,
    QPointF scenepos,
    const Process::LayerContextMenuManager& cm)
{
  auto& ctx = m_context.context;
  auto& actions = ctx.app.actions;

  // Get ScenarioModel actions
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

  auto createCommentAct = new QAction{"Add a Comment Block", &menu};
  connect(createCommentAct, &QAction::triggered, [&, scenepos, pos]() {
    auto scenPoint = Scenario::ConvertToScenarioPoint(
          scenepos, zoomRatio(), view().height());

    auto cmd = new Scenario::Command::CreateCommentBlock{
        m_layer,
        scenPoint.date, scenPoint.y};
    CommandDispatcher<>{ctx.commandStack}.submitCommand(cmd);
  });

  menu.addAction(createCommentAct);
}

void TemporalScenarioPresenter::drawDragLine(const StateModel& st, Point pt) const
{
  auto& real_st = m_states.at(st.id());
  m_view->drawDragLine(real_st.view()->pos(), { pt.date.toPixels(m_zoomRatio), pt.y * m_view->height() });
}

void TemporalScenarioPresenter::stopDrawDragLine() const
{
  m_view->stopDrawDragLine();
}

template <typename Map, typename Id>
void TemporalScenarioPresenter::removeElement(Map& map, const Id& id)
{
  auto it = map.find(id);
  if (it != map.end())
  {
    delete *it;
    map.erase(it);
  }

  m_view->update();
}

void TemporalScenarioPresenter::on_stateRemoved(const StateModel& state)
{
  removeElement(m_states.get(), state.id());
}

void TemporalScenarioPresenter::on_eventRemoved(const EventModel& event)
{
  removeElement(m_events.get(), event.id());
}

void TemporalScenarioPresenter::on_timeSyncRemoved(
    const TimeSyncModel& timeSync)
{
  removeElement(m_timeSyncs.get(), timeSync.id());
}

void TemporalScenarioPresenter::on_intervalRemoved(
    const IntervalModel& cvm)
{
  // Don't put a const auto& here, else deletion will crash.
  for (auto& pres : m_intervals)
  {
    // OPTIMIZEME add an index in the map on viewmodel id ?
    if (pres.id() == cvm.id())
    {
      auto cid = pres.id();
      auto it = m_intervals.find(cid);
      if (it != m_intervals.end())
      {
        m_intervals.remove(cid);
        delete &pres;
      }

      m_view->update();
      break;
    }
  }
}

void TemporalScenarioPresenter::on_commentRemoved(
    const CommentBlockModel& cmt)
{
  removeElement(m_comments.get(), cmt.id());
}

/////////////////////////////////////////////////////////////////////
// USER INTERACTIONS
void TemporalScenarioPresenter::on_askUpdate()
{
  m_view->update();
}

void TemporalScenarioPresenter::on_keyPressed(int k)
{
  emit keyPressed(k);
}

void TemporalScenarioPresenter::on_keyReleased(int k)
{
  emit keyReleased(k);
}

void TemporalScenarioPresenter::on_intervalExecutionTimer()
{
  for (TemporalIntervalPresenter& cst : m_intervals)
  {
    auto pp = cst.model().duration.playPercentage();

    if(double w = cst.on_playPercentageChanged(pp))
    {
      auto& v = *cst.view();
      const auto r = v.boundingRect();

      if(r.width() > 7.)
        v.update(r.x() + v.playWidth() - w, r.y(), 2 * w, 5.);
      else if(pp == 0.)
        v.update();
    }
  }
}

void TemporalScenarioPresenter::doubleClick(QPointF pt)
{
  auto sp = toScenarioPoint(pt);

  // Just create a dot
  auto cmd
      = new Command::CreateTimeSync_Event_State{m_layer, sp.date, sp.y};
  CommandDispatcher<>{m_context.context.commandStack}.submitCommand(cmd);
}

void TemporalScenarioPresenter::on_focusChanged()
{
  if (focused())
  {
    m_view->setFocus();
  }

  editionSettings().setTool(Scenario::Tool::Select);
  editionSettings().setExpandMode(ExpandMode::Scale);
}

/////////////////////////////////////////////////////////////////////
// ELEMENTS CREATED
void TemporalScenarioPresenter::on_eventCreated(const EventModel& event_model)
{
  auto ev_pres = new EventPresenter{event_model, m_view, this};
  m_events.insert(ev_pres);

  ev_pres->view()->setWidthScale(m_graphicalScale);
  m_viewInterface.on_eventMoved(*ev_pres);

  con(event_model, &EventModel::extentChanged, this,
      [=](const VerticalExtent&) { m_viewInterface.on_eventMoved(*ev_pres); });
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
        &TemporalScenarioView::pressedAsked);
  connect(
        ev_pres, &EventPresenter::moved, m_view,
        &TemporalScenarioView::movedAsked);
  connect(
        ev_pres, &EventPresenter::released, m_view,
        &TemporalScenarioView::released);
}

void TemporalScenarioPresenter::on_timeSyncCreated(
    const TimeSyncModel& timeSync_model)
{
  auto tn_pres = new TimeSyncPresenter{timeSync_model, m_view, this};
  m_timeSyncs.insert(tn_pres);

  m_viewInterface.on_timeSyncMoved(*tn_pres);

  con(timeSync_model, &TimeSyncModel::extentChanged, this,
      [=](const VerticalExtent&) {
    m_viewInterface.on_timeSyncMoved(*tn_pres);
  });
  con(timeSync_model, &TimeSyncModel::dateChanged, this,
      [=](const TimeVal&) { m_viewInterface.on_timeSyncMoved(*tn_pres); });

  // For the state machine
  connect(
        tn_pres, &TimeSyncPresenter::pressed, m_view,
        &TemporalScenarioView::pressedAsked);
  connect(
        tn_pres, &TimeSyncPresenter::moved, m_view,
        &TemporalScenarioView::movedAsked);
  connect(
        tn_pres, &TimeSyncPresenter::released, m_view,
        &TemporalScenarioView::released);
}

void TemporalScenarioPresenter::on_stateCreated(const StateModel& state)
{
  auto st_pres = new StatePresenter{state, m_view, this};
  m_states.insert(st_pres);

  st_pres->view()->setScale(m_graphicalScale);
  m_viewInterface.on_stateMoved(*st_pres);

  con(state, &StateModel::heightPercentageChanged, this,
      [=]() { m_viewInterface.on_stateMoved(*st_pres); });

  // For the state machine
  connect(
        st_pres, &StatePresenter::pressed, m_view,
        &TemporalScenarioView::pressedAsked);
  connect(
        st_pres, &StatePresenter::moved, m_view,
        &TemporalScenarioView::movedAsked);
  connect(
        st_pres, &StatePresenter::released, m_view,
        &TemporalScenarioView::released);

  connect(
        st_pres, &StatePresenter::askUpdate, this,
        &TemporalScenarioPresenter::on_askUpdate);
}

void TemporalScenarioPresenter::on_intervalCreated(
    const IntervalModel& interval)
{
  auto cst_pres = new TemporalIntervalPresenter{
      interval, m_context.context, true, m_view, this};
  m_intervals.insert(cst_pres);
  cst_pres->on_zoomRatioChanged(m_zoomRatio);

  m_viewInterface.on_intervalMoved(*cst_pres);

  connect(
        cst_pres, &TemporalIntervalPresenter::heightPercentageChanged, this,
        [=]() { m_viewInterface.on_intervalMoved(*cst_pres); });
  con(interval, &IntervalModel::dateChanged, this,
      [=](const TimeVal&) {
    m_viewInterface.on_intervalMoved(*cst_pres);
  });
  connect(
        cst_pres, &TemporalIntervalPresenter::askUpdate, this,
        &TemporalScenarioPresenter::on_askUpdate);

  connect(cst_pres, &TemporalIntervalPresenter::intervalHoverEnter, [=]() {
    m_viewInterface.on_hoverOnInterval(cst_pres->model().id(), true);
  });
  connect(cst_pres, &TemporalIntervalPresenter::intervalHoverLeave, [=]() {
    m_viewInterface.on_hoverOnInterval(cst_pres->model().id(), false);
  });

  // For the state machine
  connect(
        cst_pres, &TemporalIntervalPresenter::pressed, m_view,
        &TemporalScenarioView::pressedAsked);
  connect(
        cst_pres, &TemporalIntervalPresenter::moved, m_view,
        &TemporalScenarioView::movedAsked);
  connect(
        cst_pres, &TemporalIntervalPresenter::released, m_view,
        &TemporalScenarioView::released);
}

void TemporalScenarioPresenter::on_commentCreated(
    const CommentBlockModel& comment_block_model)
{
  using namespace Scenario::Command;
  auto cmt_pres = new CommentBlockPresenter{comment_block_model, m_view, this};

  m_comments.insert(cmt_pres);
  m_viewInterface.on_commentMoved(*cmt_pres);

  con(comment_block_model, &CommentBlockModel::dateChanged, this,
      [=](const TimeVal&) { m_viewInterface.on_commentMoved(*cmt_pres); });
  con(comment_block_model, &CommentBlockModel::heightPercentageChanged, this,
      [=](double y) { m_viewInterface.on_commentMoved(*cmt_pres); });

  // Selection
  connect(cmt_pres, &CommentBlockPresenter::selected, this, [&]() {
    m_selectionDispatcher.setAndCommit({&comment_block_model});
  });

  // Commands
  connect(cmt_pres, &CommentBlockPresenter::moved, this, [&](QPointF scenPos) {
    auto pos = Scenario::ConvertToScenarioPoint(
          scenPos, m_zoomRatio, m_view->height());
    m_ongoingDispatcher.submitCommand<MoveCommentBlock>(
          m_layer, comment_block_model.id(), pos.date, pos.y);
  });
  connect(
        cmt_pres, &CommentBlockPresenter::released, this,
        [&](QPointF scenPos) { m_ongoingDispatcher.commit(); });

  connect(
        cmt_pres, &CommentBlockPresenter::editFinished, this,
        [&](const QString& doc) {
    if (focused() && doc != comment_block_model.content())
    {
      CommandDispatcher<> c{m_context.context.commandStack};
      c.submitCommand(new SetCommentText{{comment_block_model}, doc});
    }
  });
}

void TemporalScenarioPresenter::updateAllElements()
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
}
}

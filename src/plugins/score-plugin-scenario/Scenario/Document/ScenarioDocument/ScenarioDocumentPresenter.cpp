// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioDocumentPresenter.hpp"

#include "ZoomPolicy.hpp"

#include <Dataflow/Commands/EditConnection.hpp>
#include <Library/Panel/LibraryPanelDelegate.hpp>
#include <Library/ProcessWidget.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Process/TimeValue.hpp>

#include <score/actions/Toolbar.hpp>
#include <score/actions/ToolbarManager.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/path/ObjectIdentifier.hpp>
#include <score/model/path/ObjectPath.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/plugins/documentdelegate/DocumentDelegatePresenter.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/statemachine/GraphicsSceneToolPalette.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/Clamp.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/actions/ActionManager.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/view/Window.hpp>

#include <ossia-qt/invoke.hpp>
#include <ossia/detail/math.hpp>

#include <QApplication>
#include <QDebug>
#include <QMenu>
#include <QScrollBar>
#include <QSize>
#include <QToolBar>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsPresenter.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProviderList.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactoryList.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalPresenter.hpp>
#include <Scenario/Document/Interval/FullView/NodalIntervalView.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/LayerData.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Document/Minimap/Minimap.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioScene.hpp>
#include <Scenario/Document/TimeRuler/TimeRuler.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::ScenarioDocumentPresenter)
namespace Scenario
{

const ScenarioDocumentModel& ScenarioDocumentPresenter::model() const noexcept
{
  return static_cast<const ScenarioDocumentModel&>(m_model);
}

ZoomRatio ScenarioDocumentPresenter::zoomRatio() const noexcept
{
  return m_zoomRatio;
}

ScenarioDocumentView& ScenarioDocumentPresenter::view() const noexcept
{
  return safe_cast<ScenarioDocumentView&>(m_view);
}

ScenarioDocumentPresenter::ScenarioDocumentPresenter(
    const score::DocumentContext& ctx,
    score::DocumentPresenter* parent_presenter,
    const score::DocumentDelegateModel& delegate_model,
    score::DocumentDelegateView& delegate_view)
    : DocumentDelegatePresenter{parent_presenter, delegate_model, delegate_view}
    , m_selectionDispatcher{ctx.selectionStack}
    , m_focusManager{ctx.document.focusManager()}
    , m_context{ctx, m_dataflow, m_focusDispatcher}
{
  using namespace score;

  // Setup the connections
  if (auto win = static_cast<score::View*>(score::GUIAppContext().mainWindow))
  {
    connect(
        win,
        &View::sizeChanged,
        this,
        &ScenarioDocumentPresenter::on_windowSizeChanged,
        Qt::QueuedConnection);
    connect(
        win,
        &View::ready,
        this,
        &ScenarioDocumentPresenter::on_viewReady,
        Qt::QueuedConnection);

    if(auto scroll_act = ctx.app.actions.action<Actions::AutoScroll>(); scroll_act.action())
    {
      m_autoScroll = scroll_act.action()->isChecked();
    }

  }

  con(view().view(),
      &ProcessGraphicsView::sizeChanged,
      this,
      &ScenarioDocumentPresenter::on_windowSizeChanged,
      Qt::QueuedConnection);

  con(view().view(), &ProcessGraphicsView::visibleRectChanged,
      this, &ScenarioDocumentPresenter::on_visibleRectChanged);

  con(view().view(),
      &ProcessGraphicsView::horizontalZoom,
      this,
      &ScenarioDocumentPresenter::on_horizontalZoom);
  con(view().view(),
      &ProcessGraphicsView::verticalZoom,
      this,
      &ScenarioDocumentPresenter::on_verticalZoom);
  con(view().view(),
      &ProcessGraphicsView::scrolled,
      this,
      &ScenarioDocumentPresenter::on_horizontalPositionChanged);

  con(view(),
      &ScenarioDocumentView::setLargeView,
      this,
      &ScenarioDocumentPresenter::setLargeView);

  con(view().timeRuler(),
      &TimeRuler::drag,
      this,
      &ScenarioDocumentPresenter::on_timeRulerScrollEvent);
  con(view(), &ScenarioDocumentView::timeRulerChanged,
      this, &ScenarioDocumentPresenter::on_timeRulerChanged);

  con(view().minimap(),
      &Minimap::visibleRectChanged,
      this,
      &ScenarioDocumentPresenter::on_minimapChanged);

  // Focus
  connect(
      &m_focusDispatcher,
      qOverload<QPointer<Process::LayerPresenter>>(&FocusDispatcher::focus),
      this,
      &ScenarioDocumentPresenter::setFocusedPresenter,
      Qt::QueuedConnection);

  auto& set = ctx.app.settings<Settings::Model>();
  con(set, &Settings::Model::GraphicZoomChanged, this, [&](double d) {
    auto& skin = Process::Style::instance();
    skin.setIntervalWidth(d);
  });
  con(set,
      &Settings::Model::TimeBarChanged,
      this,
      &ScenarioDocumentPresenter::updateTimeBar);

  // Help for the FocusDispatcher.
  connect(
      this,
      &ScenarioDocumentPresenter::setFocusedPresenter,
      &m_focusManager,
      static_cast<void (Process::ProcessFocusManager::*)(
          QPointer<Process::LayerPresenter>)>(
          &Process::ProcessFocusManager::focus));

  con(m_focusManager,
      &Process::ProcessFocusManager::sig_defocusedViewModel,
      this,
      &ScenarioDocumentPresenter::on_viewModelDefocused);
  con(m_focusManager,
      &Process::ProcessFocusManager::sig_focusedViewModel,
      this,
      &ScenarioDocumentPresenter::on_viewModelFocused);
  con(
      m_focusManager,
      &Process::ProcessFocusManager::sig_focusedRoot,
      this,
      [] {
        ScenarioApplicationPlugin& app
            = score::GUIAppContext()
                  .guiApplicationPlugin<ScenarioApplicationPlugin>();
        app.editionSettings().setExpandMode(ExpandMode::GrowShrink);
      },
      Qt::QueuedConnection);

  // Execution timers
  con(m_context.execTimer, &QTimer::timeout,
      this, &ScenarioDocumentPresenter::on_executionTimer);

  // Nodal mode control
  if (auto tb
      = ctx.app.toolbars.get().find(StringKey<score::Toolbar>("UISetup"));
      tb != ctx.app.toolbars.get().end())
  {
    // Nodal stuff
    auto actions = tb->second.toolbar()->actions();
    SCORE_ASSERT(!actions.empty());

    m_timelineAction = actions[0];
    SCORE_ASSERT(m_timelineAction);

    connect(m_timelineAction, &QAction::toggled,
            this, &ScenarioDocumentPresenter::on_timelineModeSwitch);

    m_musicalAction = actions[1];
  }

  // Library double-click
  if (auto processLib = ctx.app.findPanel<Library::ProcessPanel>())
  {
    con(processLib->processWidget().processView(),
        &Library::ProcessTreeView::doubleClicked,
        this,
        &ScenarioDocumentPresenter::on_addProcessFromLibrary);
  }


  setDisplayedInterval(&model().baseInterval());

  model()
      .cables.mutable_added.connect<&ScenarioDocumentPresenter::on_cableAdded>(
          *this);
  model()
      .cables.removing.connect<&ScenarioDocumentPresenter::on_cableRemoving>(
          *this);
}

void ScenarioDocumentPresenter::recenterNodal()
{
  struct {
    void operator()(CentralIntervalDisplay& disp) const noexcept {
    }
    void operator()(CentralNodalDisplay& disp) const noexcept {
      disp.recenter();
    }
    void operator()(std::monostate) const noexcept {}
  } vis{};

  std::visit(vis, m_centralDisplay);
}

void ScenarioDocumentPresenter::switchMode(bool nodal)
{
  const auto mode = nodal ? IntervalModel::ViewMode::Nodal
                          : IntervalModel::ViewMode::Temporal;
  displayedInterval().setViewMode(mode);

  view().view().setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  view().view().setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  view().view().setDragMode(QGraphicsView::NoDrag);
  view().view().setContextMenuPolicy(Qt::DefaultContextMenu);

  // First clear the display
  m_centralDisplay.emplace<std::monostate>();

  // Then reconstruct depending on the mode we want
  if (nodal)
  {
    view().view().timebarVisible = false;

    m_centralDisplay.emplace<CentralNodalDisplay>(*this);
  }
  else
  {
    m_centralDisplay.emplace<CentralIntervalDisplay>(*this);
    if(view().view().timebarPlaying)
      startTimeBar();

    // It may happen that the score is closed and this event is called
    // and the event is processed before this is deleted, but after the interval
    // is, so we have to double-check
    QPointer<IntervalModel> itv = &displayedElements.interval();
    QTimer::singleShot(0, this, [=] {
      if(itv)
        restoreZoom();
    });
  }


  {
    const struct {
      void operator()(CentralIntervalDisplay& disp) const noexcept {
        disp.init();
      }
      void operator()(CentralNodalDisplay& disp) const noexcept {
        disp.init();
      }
      void operator()(std::monostate) const noexcept {
      }
    } init_vis{};
    std::visit(init_vis, m_centralDisplay);
  }

  /*
  for(auto& cable : m_dataflow.cables())
  {
    cable.second->check();
  }
  */
  view().showRulers(!nodal);
}

ScenarioDocumentPresenter::~ScenarioDocumentPresenter()
{
  delete m_miniLayer;

  m_centralDisplay.emplace<std::monostate>();

  for(auto& [cable, item] : m_dataflow.cables())
    delete item;
  m_dataflow.cables().clear();
  SCORE_ASSERT(m_dataflow.ports().empty());
  m_dataflow.ports().clear();
}

IntervalModel& ScenarioDocumentPresenter::displayedInterval() const noexcept
{
  return displayedElements.interval();
}

IntervalPresenter* ScenarioDocumentPresenter::displayedIntervalPresenter() const noexcept
{
  if(auto itv_pres = std::get_if<CentralIntervalDisplay>(&m_centralDisplay))
    return itv_pres->presenter.intervalPresenter();
  return nullptr;
}

void ScenarioDocumentPresenter::selectAll()
{
  auto processmodel = focusManager().focusedModel();
  if (processmodel)
  {
    m_selectionDispatcher.select(processmodel->selectableChildren());
  }
}

void ScenarioDocumentPresenter::deselectAll()
{
  m_selectionDispatcher.deselect();
}

const CentralDisplay& ScenarioDocumentPresenter::display() const noexcept
{
  return m_centralDisplay;
}

void ScenarioDocumentPresenter::selectTop()
{
  focusManager().focus(this);
  score::SelectionDispatcher{m_context.selectionStack}.select(Selection{
      &displayedElements.startState(),
      &displayedElements.interval(),
      &displayedElements.endState()});
}

void ScenarioDocumentPresenter::setZoomRatio(ZoomRatio newRatio)
{
  m_zoomRatio = newRatio;

  view().timeRuler().setZoomRatio(newRatio);

  const struct {
    ZoomRatio zoomRatio{};
    void operator()(CentralIntervalDisplay& disp) const noexcept {
      disp.presenter.on_zoomRatioChanged(zoomRatio);
    }
    void operator()(CentralNodalDisplay& disp) const noexcept {
    }
    void operator()(std::monostate) const noexcept {
    }
  } vis{m_zoomRatio};
  std::visit(vis, m_centralDisplay);

  for (auto& cbl : m_dataflow.cables())
  {
    if (cbl.second)
      cbl.second->resize();
  }
}

void ScenarioDocumentPresenter::on_horizontalZoom(
    QPointF zoom,
    QPointF scenePoint)
{
  auto& map = view().minimap();

  // Position in pixels of the scroll in the viewport
  const double x_view = view().view().mapFromScene(scenePoint).x();
  const auto x_view_percent = x_view / double(view().viewportRect().width());

  // Zoom while keeping the zoomed-to position constant
  auto lh = map.leftHandle();
  auto rh = map.rightHandle();

  // Zoom
  lh += 0.01 * (rh - lh) * x_view_percent * zoom.y() / 2.;
  rh -= 0.01 * (rh - lh) * (1. - x_view_percent) * zoom.y() / 2.;

  if(lh < 0 && rh > 0)
  {
    // 0.3 to make sure that we don't "dezoom" too fast.
    lh *= -1. * 0.3;
    double ratio = 1. + lh / rh;
    auto& c = displayedInterval();

    c.duration.setGuiDuration(c.duration.guiDuration() * ratio);
    lh = 0;
    rh += lh;
  }
  view().minimap().modifyHandles(lh, rh);
}

void ScenarioDocumentPresenter::on_verticalZoom(
    QPointF zoom,
    QPointF scenePoint)
{
  auto z = ossia::clamp(zoom.y(), -100., 100.);
  if (z == 0.)
    return;
  auto& c = displayedInterval();
  const FullRack& slts = c.fullView();
  for (std::size_t i = 0; i < slts.size(); i++)
  {
    SlotId slot{i, Slot::FullView};
    c.setSlotHeight(slot, c.getSlotHeight(slot) + z);
  }
}
void ScenarioDocumentPresenter::on_timeRulerScrollEvent(
    QPointF previous,
    QPointF current)
{
  view().view().scrollHorizontal(previous.x() - current.x());
}

void ScenarioDocumentPresenter::on_visibleRectChanged(const QRectF& rect)
{
  struct {
    const QRectF& rect;
    void operator()(CentralIntervalDisplay& disp) const noexcept {
      if (auto p = disp.presenter.intervalPresenter())
        p->on_visibleRectChanged(rect);
    }
    void operator()(CentralNodalDisplay& disp) const noexcept {
      disp.presenter->setRect({0, 0, rect.width(), rect.height()});
    }
    void operator()(std::monostate) const noexcept {}
  } vis{rect};

  std::visit(vis, this->m_centralDisplay);
}

void ScenarioDocumentPresenter::setLargeView()
{
  auto& c = displayedInterval();

  c.duration.setGuiDuration(c.contentDuration());

  updateMinimap();
  view().minimap().setLargeView();
}

void ScenarioDocumentPresenter::startTimeBar()
{
  bool visible = context().app.settings<Scenario::Settings::Model>().getTimeBar();
  auto itv_display = std::get_if<CentralIntervalDisplay>(&m_centralDisplay);
  visible &= bool(itv_display);
  IntervalPresenter* itv_pres{};
  if(itv_display)
  {
    itv_pres = itv_display->presenter.intervalPresenter();
    visible &= bool(itv_pres);
  }

  view().view().currentTimebar = &displayedInterval().duration;
  view().view().currentView = itv_pres ? itv_pres->view() : nullptr;
  view().view().timebarPlaying = true;
  view().view().timebarVisible = visible;

  // Necessary to redraw the exec bar correctly...
  // On non-retina macOS, FullViewportupdate on software is faster than GLWidget -=-
  // Maybe different on Retina, it has to be checked...
  if(!this->context().app.applicationSettings.opengl)
  {
    view().view().setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    m_nonGLTimebarTimer = view().startTimer(16);
  }
}

void ScenarioDocumentPresenter::stopTimeBar()
{
  view().view().currentTimebar = nullptr;
  view().view().timebarPlaying = false;
  view().view().timebarVisible = false;

  if(!this->context().app.applicationSettings.opengl)
  {
    view().view().setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    if(m_nonGLTimebarTimer != -1)
    {
      view().killTimer(m_nonGLTimebarTimer);
      m_nonGLTimebarTimer = -1;
    }
  }
}

bool ScenarioDocumentPresenter::isNodal() const noexcept
{
  return !m_timelineAction->isChecked();
}

void ScenarioDocumentPresenter::setAutoScroll(bool c)
{
  m_autoScroll = c;
}

static bool window_size_set = false;
void ScenarioDocumentPresenter::on_windowSizeChanged(QSize sz)
{
  if (m_zoomRatio == -1)
    return;

  // Keep the same zoom level with the new width.
  // Left handle should not move.
  auto new_w = view().viewWidth();

  view().timeRuler().setWidth(new_w);

  // Update the time interval if the window is greater.
  auto& c = displayedInterval();
  auto visible_rect = view().visibleSceneRect();
  if (visible_rect.width() > c.duration.guiDuration().toPixels(m_zoomRatio))
  {
    auto t = TimeVal::fromPixels(visible_rect.width(), m_zoomRatio);
    auto min_time = c.contentDuration();

    c.duration.setGuiDuration((min_time > t ? min_time : t));
  }

  updateMinimap();
  window_size_set = true;
}

void ScenarioDocumentPresenter::on_horizontalPositionChanged(int dx)
{
  // Prevent loops
  if (m_updatingView)
    return;

  // Don't update when in nodal (this breaks the zoom / position saving)
  auto itv = std::get_if<CentralIntervalDisplay>(&m_centralDisplay);
  if (!itv)
    return;

  auto& c = displayedInterval();
  auto& gv = view().view();

  if (dx < 0 && !m_zooming)
  {
    auto cur_rect = gv.mapToScene(gv.rect()).boundingRect();
    auto scene_rect = gv.sceneRect();
    if (cur_rect.x() + cur_rect.width() - dx > (scene_rect.width()))
    {
      auto t = TimeVal::fromPixels(
          cur_rect.x() + cur_rect.width() - dx, m_zoomRatio);
      c.duration.setGuiDuration(t);
      scene_rect.adjust(0, 0, 5, 0);
      gv.setSceneRect(scene_rect);
    }
  }
  else if (dx > 0 && !m_zooming)
  {
    auto min_time = c.contentDuration();
    if (min_time < c.duration.guiDuration())
    {
      auto cur_rect = gv.mapToScene(gv.rect()).boundingRect();
      auto t = std::max(
          TimeVal::fromPixels(
              cur_rect.x() + cur_rect.width() - dx, m_zoomRatio),
          min_time);
      c.duration.setGuiDuration(t);

      auto scene_rect = gv.sceneRect();
      scene_rect.adjust(0, 0, -dx, 0);
      gv.setSceneRect(scene_rect);
    }
  }

  QRectF visible_scene_rect = view().visibleSceneRect();

  view().timeRuler().setStartPoint(
      TimeVal::fromPixels(visible_scene_rect.x(), m_zoomRatio));
  const auto dur = c.duration.guiDuration();
  double center_pixel_percentage = (visible_scene_rect.center().x() / dur.toPixels(m_zoomRatio));
  c.setMidTime(dur * center_pixel_percentage);

  if (!m_updatingMinimap)
  {
    updateMinimap();
  }
}

double ScenarioDocumentPresenter::computeReverseZoom(ZoomRatio r)
{
  const auto view_width = view().viewportRect().width();
  const auto dur = displayedInterval().duration.guiDuration();

  const auto map_w = view().minimap().width();

  return map_w * r * view_width / dur.impl;
}

ZoomRatio ScenarioDocumentPresenter::computeZoom(double l, double r)
{
  const auto map_w = view().minimap().width();
  const auto view_width = view().viewportRect().width();
  const auto dur = displayedInterval().duration.guiDuration();

  // Compute new zoom level
  const auto disptime = dur.impl * ((r - l) / map_w);
  return disptime / view_width;
}

void ScenarioDocumentPresenter::on_addProcessFromLibrary(
    const Library::ProcessData& dat)
{
  if(&this->context().app.currentDocument()->document != &this->context().document)
    return;

  const struct {
    const Library::ProcessData& dat;
    void operator()(CentralIntervalDisplay& disp) const noexcept {
      disp.on_addProcessFromLibrary(dat);
    }
    void operator()(CentralNodalDisplay& disp) const noexcept {
      disp.on_addProcessFromLibrary(dat);
    }
    void operator()(std::monostate) const noexcept {
    }
  } vis{dat};
  std::visit(vis, m_centralDisplay);
}

void ScenarioDocumentPresenter::restoreZoom()
{
  if (auto z = displayedInterval().zoom(); z > 0)
  {
    auto& c = displayedInterval();

    auto& minimap = view().minimap();
    const auto viewWidth = view().viewWidth();
    minimap.setWidth(viewWidth);

    auto minimap_handle_width = computeReverseZoom(z);

    // Take into account the 10px offset of the interval
    // No matter the zoom level, there's always 10px
    double minimap_offset_ratio = TimeVal::fromPixels(10., z) / c.duration.guiDuration();
    double minimap_offset_pixels = minimap_offset_ratio * viewWidth / 2. + 1.;

    minimap_handle_width -= minimap_offset_pixels;
    const auto cstDur = displayedInterval().duration.guiDuration();
    double handles_duration_ratio = (c.midTime() / c.duration.guiDuration());
    double handle_center = handles_duration_ratio * viewWidth;
    auto lx = handle_center - minimap_handle_width / 2./* - minimap_offset_pixels*/;

    minimap.setMinDistance(2. * viewWidth / cstDur.impl);
    m_reloadingMinimap = true;
    minimap.restoreHandles(lx, lx + minimap_handle_width);
    m_reloadingMinimap = false;
  }
  else
  {
    setLargeView();
  }
}

void ScenarioDocumentPresenter::on_viewReady()
{
  QTimer::singleShot(0, this, [=] {
    restoreZoom();

    if (!window_size_set)
      on_windowSizeChanged({});

    const struct {
      QRectF rect;
      void operator()(CentralIntervalDisplay& disp) const noexcept {
        disp.on_visibleRectChanged(rect);
      }
      void operator()(CentralNodalDisplay& disp) const noexcept {
        disp.on_visibleRectChanged(rect);
      }
      void operator()(std::monostate) const noexcept {
      }
    } vis{view().view().visibleRect()};
    std::visit(vis, m_centralDisplay);
  });
}

void ScenarioDocumentPresenter::on_cableAdded(Process::Cable& c)
{
  // Run async because the cable model may have been
  // created before the port item have in e.g. an undo command
  ossia::qt::run_async(this, [this, ptr = QPointer{&c}] {
    if (ptr)
    {
      m_dataflow.createCable(*ptr, m_context, &view().scene());
    }
  });
}

void ScenarioDocumentPresenter::on_cableRemoving(const Process::Cable& c)
{
  auto it = m_dataflow.cables().find(const_cast<Process::Cable*>(&c));
  if (it != m_dataflow.cables().end())
  {
    delete it->second;
    m_dataflow.cables().erase(it);
  }
}

void ScenarioDocumentPresenter::on_timeRulerChanged()
{
  auto& tr = view().timeRuler();

  con(tr,
      &TimeRuler::drag,
      this,
      &ScenarioDocumentPresenter::on_timeRulerScrollEvent);

  if (m_zoomRatio > 0)
    tr.setZoomRatio(m_zoomRatio);
  tr.setWidth(view().viewWidth());

  struct {
    Scenario::TimeRulerBase& tr;
    void operator()(CentralIntervalDisplay& disp) const noexcept {
      if (auto p = disp.presenter.intervalPresenter())
      {
        tr.setGrid(p->grid());
        p->on_zoomRatioChanged(p->zoomRatio());
      }
    }
    void operator()(CentralNodalDisplay& disp) const noexcept {}
    void operator()(std::monostate) const noexcept {}
  } vis{tr};
  std::visit(vis, this->m_centralDisplay);

  on_horizontalPositionChanged(0);
}

void ScenarioDocumentPresenter::on_minimapChanged(double l, double r)
{
  m_updatingMinimap = true;
  auto& c = displayedInterval();
  const auto dur = c.duration.guiDuration();

  // Compute new zoom level
  // 1000 flicks per pixels -> roughly 800 pixels for one millisecond
  const auto newZoom = std::max(computeZoom(l, r), 1000.);

  // Compute new x position
  const auto newCstWidth = dur.toPixels(newZoom);
  auto view_width = view().viewportRect().width();
  const auto newX = newCstWidth * l / view_width;

  m_zooming = true;
  auto& gv = view().view();
  const auto& vp = *gv.viewport();
  const double w = vp.width();
  const double h = vp.height();

  const QRectF visible_scene_rect = view().visibleSceneRect();
  const double y = visible_scene_rect.top();

  // Set zoom
  if (newZoom != m_zoomRatio)
    setZoomRatio(newZoom);

  // Set viewport position
  auto newView = QRectF{newX, y, (qreal)w, (qreal)h};
  gv.ensureVisible(newView, 0., 0.);
  gv.viewport()->update();

  view().timeRuler().setWidth(gv.width());

  // Save state in interval
  if(!m_reloadingMinimap)
  {
    c.setZoom(newZoom);
    c.setMidTime(TimeVal(
        dur.impl
        * (view().visibleSceneRect().center().x() / dur.toPixels(newZoom))));
  }

  m_zooming = false;

  m_updatingMinimap = false;
}

void ScenarioDocumentPresenter::on_executionTimer()
{
  if(auto n = std::get_if<CentralNodalDisplay>(&this->m_centralDisplay))
  {
    n->on_executionTimer();
  }
  else if(auto i = std::get_if<CentralIntervalDisplay>(&this->m_centralDisplay))
  {
    if(m_autoScroll)
      autoScroll();
  }
}

void ScenarioDocumentPresenter::autoScroll()
{
  const auto& sel = this->context().selectionStack.currentSelection();
  auto& root = this->model().baseInterval();

  const Scenario::IntervalModel* sel_itv = &root;
  if(sel.size() == 1)
  {
    if(auto i = qobject_cast<const Scenario::IntervalModel*>(sel.at(0)))
    {
      if(i->executing())
        sel_itv = i;
    }
  }

  if(!sel_itv)
    return;

  auto& dur = sel_itv->duration;
  auto delta = Scenario::timeDelta(sel_itv, &root);
  auto visible_play_percentage = [&] {
    if(dur.isMaxInfinite())
    {
      return std::min(dur.playPercentage(), 1.);
    }
    else
    {
      return dur.playPercentage();
    }
  };

  double exec_d = visible_play_percentage() * (delta + dur.defaultDuration()).toPixels(this->m_zoomRatio);
  auto center = view().view().visibleRect().center();
  view().view().ensureVisible({exec_d - 100, center.y() - 10, view().viewWidth() / 2., 20});
}

void ScenarioDocumentPresenter::on_timelineModeSwitch(bool b)
{
  const bool nodal = !b;
  const bool m_nodal = std::get_if<CentralNodalDisplay>(&this->m_centralDisplay);
  if (nodal && !m_nodal)
    switchMode(true);
  else if (!nodal && m_nodal)
    switchMode(false);
}

void ScenarioDocumentPresenter::updateRect(const QRectF& rect)
{
  view().view().setSceneRect(rect);
}

const Process::Context& ScenarioDocumentPresenter::context() const noexcept
{
  return m_context;
}

void ScenarioDocumentPresenter::updateTimeBar()
{
  auto& set = m_context.app.settings<Settings::Model>();
  auto& gv = view().view();
  const bool nodal = std::get_if<CentralNodalDisplay>(&this->m_centralDisplay);
  gv.timebarVisible = gv.timebarPlaying && set.getTimeBar() && !nodal && (&displayedInterval().duration == gv.currentTimebar);
}

void ScenarioDocumentPresenter::updateMinimap()
{
  if (m_zoomRatio == -1)
    return;

  auto& minimap = view().minimap();

  const auto viewRect = view().viewportRect();
  const auto visibleSceneRect = view().visibleSceneRect();
  const auto viewWidth = viewRect.width();
  const auto cstDur = displayedInterval().duration.guiDuration();
  const auto cstWidth = cstDur.toPixels(m_zoomRatio);

  // ZoomRatio in the minimap view
  const auto zoomRatio = cstDur.impl / viewWidth;

  minimap.setWidth(viewWidth);
  if (m_miniLayer)
  {
    m_miniLayer->setWidth(viewWidth);
    m_miniLayer->setZoomRatio(zoomRatio);
  }

  // Compute min handle spacing.
  // The maximum zoom in the main view should be 10 pixels for one millisecond.
  // Given the viewWidth and the guiDuration, compute the distance required.
  minimap.setMinDistance(2. * viewWidth / cstDur.impl);

  // Compute handle positions.
  const auto vp_x1 = visibleSceneRect.left();
  const auto vp_x2 = visibleSceneRect.right();

  const auto lh_x = viewWidth * (vp_x1 / cstWidth);
  const auto rh_x = viewWidth * (vp_x2 / cstWidth);

  minimap.setHandles(lh_x, rh_x);
}

void ScenarioDocumentPresenter::setDisplayedInterval(IntervalModel* itv)
{
  // Can't be a ref because of a qmetatype quirk
  auto& interval = *itv;
  auto& ctx = model().context();
  if (displayedElements.initialized())
  {
    if (&interval == &displayedElements.interval())
    {
      selectTop();
      return;
    }
  }

  auto& provider = ctx.app.interfaces<DisplayedElementsProviderList>();
  DisplayedElementsContainer elements
      = provider.make(&DisplayedElementsProvider::make, interval);
  if (!elements.interval)
  {
    qWarning() << "could not put interval in fullview";
    return;
  }

  displayedElements.setDisplayedElements(std::move(elements));

  m_focusManager.focusNothing();

  disconnect(m_intervalConnection);
  disconnect(m_durationConnection);
  if (&interval != &model().baseInterval())
  {
    m_intervalConnection = con(interval, &QObject::destroyed, this, [&]() {
      setDisplayedInterval(&model().baseInterval());
    });
  }
  m_durationConnection = con(
      interval.duration, &IntervalDurations::guiDurationChanged, this, [=] {
        updateMinimap();
      });

  // Setup of the layer in the minimap
  delete m_miniLayer;
  m_miniLayer = nullptr;

  auto& layerFactoryList = ctx.app.interfaces<Process::LayerFactoryList>();
  for (auto& proc : interval.processes)
  {
    if (auto fac = layerFactoryList.findDefaultFactory(proc))
    {
      if ((m_miniLayer = fac->makeMiniLayer(proc, nullptr)))
      {
        m_miniLayer->setHeight(40);
        m_miniLayer->setWidth(view().minimap().width());
        view().minimap().scene()->addItem(m_miniLayer);
        con(proc,
            &Process::ProcessModel::identified_object_destroying,
            this,
            [=] {
              delete m_miniLayer;
              m_miniLayer = nullptr;
            });
        break;
      }
    }
  }

  // Setup of the nodal stuff
  {
    if (m_timelineAction)
    {
      switch (interval.viewMode())
      {
        case Scenario::IntervalModel::Temporal:
          m_timelineAction->setChecked(true);
          break;
        case Scenario::IntervalModel::Nodal:
          m_timelineAction->setChecked(false);
          break;
      }
    }
    switchMode(interval.viewMode() == IntervalModel::ViewMode::Nodal);
  }
}

void ScenarioDocumentPresenter::on_viewModelDefocused(
    const Process::ProcessModel* vm)
{
  // Deselect
  // Note : why these two lines ?
  // selectionStack.clear() should clear the selection everywhere anyway.
  if (vm)
    vm->setSelection({});

  score::IDocument::documentContext(*this).selectionStack.clearAllButLast();
}

void ScenarioDocumentPresenter::on_viewModelFocused(
    const Process::ProcessModel* process)
{
  // If the parent of the layer is a interval, we set the focus on the
  // interval too.
  auto slot = process->parent();
  if (!slot)
    return;
  auto rack = slot->parent();
  if (!rack)
    return;
  auto cm = rack->parent();
  if (auto interval = dynamic_cast<IntervalModel*>(cm))
  {
    if (m_focusedInterval)
      m_focusedInterval->focusChanged(false);

    m_focusedInterval = interval;
    m_focusedInterval->focusChanged(true);
  }
}

void ScenarioDocumentPresenter::focusFrontProcess()
{
  // TODO this snippet is useful, put it somewhere in some Toolkit file.
  auto itv_disp = std::get_if<CentralIntervalDisplay>(&m_centralDisplay);
  if(!itv_disp)
    return;
  FullViewIntervalPresenter* cst_pres
      = itv_disp->presenter.intervalPresenter();

  if (cst_pres && !cst_pres->getSlots().empty())
  {
    auto& firstSlot = cst_pres->getSlots().front();
    if (auto slt = firstSlot.getLayerSlot())
    {
      if(!slt->layers.empty())
      {
        if (auto p = slt->layers.front().mainPresenter())
          focusManager().focus(p);
      }
      else
      {
        qDebug() << "Warning: trying to focus slot with no layers";
      }
    }
    else
    {
      qDebug() << "Warning: trying to focus non-layer slot";
    }
  }
}

void ScenarioDocumentPresenter::goUpALevel()
{
  if (!this->displayedElements.initialized())
    return;

  QObject* itv = &this->displayedElements.interval();
  while (itv)
  {
    itv = itv->parent();
    if (auto itv_ = qobject_cast<const IntervalModel*>(itv))
    {
      setDisplayedInterval(const_cast<IntervalModel*>(itv_));
      return;
    }
  }
}

void ScenarioDocumentPresenter::setNewSelection(
    const Selection& old,
    const Selection& s)
{
  auto process = m_focusManager.focusedModel();
  auto clearProcessSelection = [=] (Process::ProcessModel* process) {
    if (process)
    {
      process->setSelection(Selection{});
      process->selection.set(false);
      for(auto& con : m_processSelectionConnections)
        QObject::disconnect(con);
      m_processSelectionConnections.clear();
    }
  };

  for (auto& e : old)
  {
    const auto it = ossia::find(s, e);
    if (it == s.end())
    {
      if (auto proc = qobject_cast<Process::ProcessModel*>(e))
      {
        proc->selection.set(false);
      }
      else if (auto port = qobject_cast<Process::Port*>(e))
      {
        port->selection.set(false);
        if(auto proc = qobject_cast<Process::ProcessModel*>(port->parent()))
          proc->selection.set(false);
      }
      else if (auto cable = qobject_cast<Process::Cable*>(e))
      {
        cable->selection.set(false);
      }
    }
  }

  // Manages the selection (different case if we're
  // selecting something in a process, or something in full view)
  if (s.empty())
  {
    clearProcessSelection(process);
    displayedElements.setSelection(Selection{});

    // Note : once here was a call to defocus a presenter. Why ? See git blame.
  }
  else if (ossia::any_of(s, [&](const QObject* obj) {
             return obj == &displayedElements.interval()
                    || obj == &displayedElements.startTimeSync()
                    || obj == &displayedElements.endTimeSync()
                    || obj == &displayedElements.startEvent()
                    || obj == &displayedElements.endEvent()
                    || obj == &displayedElements.startState()
                    || obj == &displayedElements.endState();
           }))
  {
    clearProcessSelection(process);

    auto pres = score::IDocument::get<Scenario::ScenarioDocumentPresenter>(
        *score::IDocument::documentFromObject(this));
    if (pres)
      m_focusManager.focus(pres);

    displayedElements.setSelection(s);
  }
  else
  {
    displayedElements.setSelection(Selection{});

    // We know by the presenter that all objects
    // in a given selection are in the same Process.
    std::vector<Process::ProcessModel*> selectedProcesses;
    for(auto& elt : s)
    {
      if(auto obj = qobject_cast<Process::ProcessModel*>(elt))
        selectedProcesses.push_back(obj);
    }

    if(selectedProcesses.empty())
    {
      // Select child of processes
      auto newProc = Process::parentProcess(*s.begin());
      if(newProc)
      {
        if(process && process != newProc)
        {
          clearProcessSelection(process);
        }

        newProc->setSelection(s);
        if (process)
        {
          process->selection.set(false);
        }
        newProc->selection.set(true);

        m_processSelectionConnections.push_back(connect(
            newProc,
            &Process::ProcessModel::identified_object_destroying,
            this,
            &ScenarioDocumentPresenter::deselectAll,
            Qt::UniqueConnection));
      }
      else
      {
        qDebug() << "Weird case ?? " << (QObject*)s.begin()->data();
        if(process)
          clearProcessSelection(process);
      }
    }
    else
    {
      if (process && !ossia::contains(selectedProcesses, process))
      {
        clearProcessSelection(process);
      }

      for(auto newProc : selectedProcesses)
      {
        newProc->selection.set(true);
        m_processSelectionConnections.push_back(connect(
            newProc,
            &Process::ProcessModel::identified_object_destroying,
            this,
            &ScenarioDocumentPresenter::deselectAll,
            Qt::UniqueConnection));
      }
    }

    for (auto& elt : s)
    {
      if (auto cable = qobject_cast<Process::Cable*>(elt.data()))
        cable->selection.set(true);
      else if (auto port = qobject_cast<Process::Port*>(elt.data()))
        port->selection.set(true);
    }
  }

  view().view().setFocus();
}

Process::ProcessFocusManager& ScenarioDocumentPresenter::focusManager() const noexcept
{
  return m_focusManager;
}
}

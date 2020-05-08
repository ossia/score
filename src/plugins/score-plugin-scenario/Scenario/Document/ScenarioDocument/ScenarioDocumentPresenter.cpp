// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioDocumentPresenter.hpp"

#include "ZoomPolicy.hpp"

#include <Dataflow/Commands/EditConnection.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Process/TimeValue.hpp>
#include <Process/Tools/ProcessGraphicsView.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsPresenter.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProviderList.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactoryList.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Document/Minimap/Minimap.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioScene.hpp>
#include <Scenario/Document/TimeRuler/TimeRuler.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

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
#include <score/tools/Clamp.hpp>
#include <score/tools/Bind.hpp>
#include <ossia-qt/invoke.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <core/application/ApplicationSettings.hpp>

#include <core/view/Window.hpp>

#include <ossia/detail/math.hpp>

#include <QDebug>
#include <QSize>
#include <QScrollBar>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::ScenarioDocumentPresenter)
namespace Scenario
{
const ScenarioDocumentModel& ScenarioDocumentPresenter::model() const
{
  return static_cast<const ScenarioDocumentModel&>(m_model);
}

ZoomRatio ScenarioDocumentPresenter::zoomRatio() const
{
  return m_zoomRatio;
}

ScenarioDocumentView& ScenarioDocumentPresenter::view() const
{
  return safe_cast<ScenarioDocumentView&>(m_view);
}

ScenarioDocumentPresenter::ScenarioDocumentPresenter(
    const score::DocumentContext& ctx,
    score::DocumentPresenter* parent_presenter,
    const score::DocumentDelegateModel& delegate_model,
    score::DocumentDelegateView& delegate_view)
    : DocumentDelegatePresenter{parent_presenter,
                                delegate_model,
                                delegate_view}
    , m_scenarioPresenter{*this}
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
  }

  con(view().view(),
      &ProcessGraphicsView::sizeChanged,
      this,
      &ScenarioDocumentPresenter::on_windowSizeChanged,
      Qt::QueuedConnection);

  con(view().view(),
      &ProcessGraphicsView::visibleRectChanged, this, [&] (QRectF rect) {
    if(auto p = presenters().intervalPresenter())
      p->on_visibleRectChanged(rect);
  });
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

  connect(
      &m_scenarioPresenter,
      &DisplayedElementsPresenter::requestFocusedPresenterChange,
      &focusManager(),
      static_cast<void (Process::ProcessFocusManager::*)(
          QPointer<Process::LayerPresenter>)>(
          &Process::ProcessFocusManager::focus));

  con(view().timeRuler(),
      &TimeRuler::drag,
      this,
      &ScenarioDocumentPresenter::on_timeRulerScrollEvent);

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
  con(m_focusManager,
      &Process::ProcessFocusManager::sig_focusedRoot,
      this,
      [] {
        ScenarioApplicationPlugin& app
            = score::GUIAppContext()
                  .guiApplicationPlugin<ScenarioApplicationPlugin>();
        app.editionSettings().setExpandMode(ExpandMode::GrowShrink);
      },
      Qt::QueuedConnection);

  con(m_context.coarseUpdateTimer, &QTimer::timeout, this, [&] {
    auto pctg = displayedInterval().duration.playPercentage();
    auto& itv = *presenters().intervalPresenter()->view();
    auto x = pctg * itv.defaultWidth() + itv.pos().x();
    if(x != view().timeBar().x())
      view().timeBar().setPos(x, 0);
  });

  setDisplayedInterval(model().baseInterval());

  model()
      .cables.mutable_added.connect<&ScenarioDocumentPresenter::on_cableAdded>(
          *this);
  model()
      .cables.removing.connect<&ScenarioDocumentPresenter::on_cableRemoving>(
          *this);
}

ScenarioDocumentPresenter::~ScenarioDocumentPresenter()
{
  m_dataflow.cables().clear();
  m_dataflow.ports().clear();
}

IntervalModel& ScenarioDocumentPresenter::displayedInterval() const
{
  return displayedElements.interval();
}

const DisplayedElementsPresenter& ScenarioDocumentPresenter::presenters() const
{
  return m_scenarioPresenter;
}

void ScenarioDocumentPresenter::selectAll()
{
  auto processmodel = focusManager().focusedModel();
  if (processmodel)
  {
    m_selectionDispatcher.setAndCommit(processmodel->selectableChildren());
  }
}

void ScenarioDocumentPresenter::deselectAll()
{
  m_selectionDispatcher.setAndCommit(Selection{});
}

void ScenarioDocumentPresenter::selectTop()
{
  focusManager().focus(this);
  score::SelectionDispatcher{m_context.selectionStack}.setAndCommit(
      {&displayedElements.startState(),
       &displayedElements.interval(),
       &displayedElements.endState()});
}

void ScenarioDocumentPresenter::setZoomRatio(ZoomRatio newRatio)
{
  m_zoomRatio = newRatio;

  view().timeRuler().setZoomRatio(newRatio);
  m_scenarioPresenter.on_zoomRatioChanged(m_zoomRatio);

  for (auto& cbl : m_dataflow.cables())
  {
    if(cbl.second)
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

void ScenarioDocumentPresenter::setLargeView()
{
  auto& c = displayedInterval();

  c.duration.setGuiDuration(c.contentDuration());

  updateMinimap();
  view().minimap().setLargeView();
}

void ScenarioDocumentPresenter::startTimeBar(IntervalModel& itv)
{
  auto& bar = view().timeBar();
  bar.setVisible(
      context().app.settings<Scenario::Settings::Model>().getTimeBar());
  view().timeBar().playing = true;
  view().timeBar().setInterval(&itv);
}

void ScenarioDocumentPresenter::stopTimeBar()
{
  auto& bar = view().timeBar();
  bar.setVisible(false);
  bar.playing = false;
  bar.setInterval(nullptr);
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
  if (m_updatingView)
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
            cur_rect.x() + cur_rect.width() - dx,
            m_zoomRatio);
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
          TimeVal::fromPixels(cur_rect.x() + cur_rect.width() - dx, m_zoomRatio),
          min_time);
      c.duration.setGuiDuration(t);

      auto scene_rect = gv.sceneRect();
      scene_rect.adjust(0, 0, -dx, 0);
      gv.setSceneRect(scene_rect);
    }
  }

  QRectF visible_scene_rect = view().visibleSceneRect();

  view().timeRuler().setStartPoint(TimeVal::fromPixels(visible_scene_rect.x(), m_zoomRatio));
  const auto dur = c.duration.guiDuration();
  c.setMidTime(
      dur * (visible_scene_rect.center().x() / dur.toPixels(m_zoomRatio)));

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

void ScenarioDocumentPresenter::on_viewReady()
{
  QTimer::singleShot(0, [=] {
    auto z = displayedInterval().zoom();
    if (z > 0)
    {
      auto& c = displayedInterval();
      auto minimap_handle_width = computeReverseZoom(z);
      auto rx
          = (c.midTime() / c.duration.guiDuration()) * view().minimap().width()
            - minimap_handle_width / 2.;
      view().minimap().modifyHandles(rx, rx + minimap_handle_width);
    }
    else
    {
      setLargeView();
    }

    if (!window_size_set)
      on_windowSizeChanged({});
  });
}

void ScenarioDocumentPresenter::on_cableAdded(Process::Cable& c)
{
  // Run async because the cable model may have been
  // created before the port item have in e.g. an undo command
  ossia::qt::run_async(
        this,
        [this, ptr = QPointer{&c}] {
    if(ptr)
    {
      auto it = new Dataflow::CableItem{*ptr, m_context, nullptr};
      if(!it->parentItem())
        view().scene().addItem(it);
    }
  });
}

void ScenarioDocumentPresenter::on_cableRemoving(const Process::Cable& c)
{
  auto it = m_dataflow.cables().find(const_cast<Process::Cable*>(&c));
  if(it != m_dataflow.cables().end())
  {
    delete it->second;
    m_dataflow.cables().erase(it);
  }
}

void ScenarioDocumentPresenter::on_minimapChanged(double l, double r)
{
  m_updatingMinimap = true;
  auto& c = displayedInterval();
  const auto dur = c.duration.guiDuration();

  // Compute new zoom level
  const auto newZoom = computeZoom(l, r);

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
  c.setZoom(newZoom);
  c.setMidTime(
      TimeVal(dur.impl * (view().visibleSceneRect().center().x() / dur.toPixels(newZoom))));

  m_zooming = false;

  m_updatingMinimap = false;
}

void ScenarioDocumentPresenter::updateRect(const QRectF& rect)
{
  view().view().setSceneRect(rect);
}

const Process::Context&
ScenarioDocumentPresenter::context() const
{
  return m_context;
}

void ScenarioDocumentPresenter::updateTimeBar()
{
  auto& set = m_context.app.settings<Settings::Model>();
  view().timeBar().setVisible(
      view().timeBar().playing && set.getTimeBar()
      && (&m_scenarioPresenter.intervalPresenter()->model()
          == view().timeBar().interval()));
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
  minimap.setMinDistance(2 * viewWidth / cstDur.impl);

  // Compute handle positions.
  const auto vp_x1 = visibleSceneRect.left();
  const auto vp_x2 = visibleSceneRect.right();

  const auto lh_x = viewWidth * (vp_x1 / cstWidth);
  // minimap.setLeftHandle(lh_x);

  const auto rh_x = viewWidth * (vp_x2 / cstWidth);
  minimap.setHandles(lh_x, rh_x);
  // minimap.setRightHandle(rh_x);
}

void ScenarioDocumentPresenter::setDisplayedInterval(IntervalModel& interval)
{
  auto& ctx = score::IDocument::documentContext(model());
  if (displayedElements.initialized())
  {
    if (&interval == &displayedElements.interval())
    {
      selectTop();
      return;
    }
  }

  auto& provider = ctx.app.interfaces<DisplayedElementsProviderList>();
  DisplayedElementsContainer elements = provider.make(&DisplayedElementsProvider::make, interval);
  if(!elements.interval)
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
      setDisplayedInterval(model().baseInterval());
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

  // Setup of the state machine.
  const auto& fact
      = ctx.app.interfaces<DisplayedElementsToolPaletteFactoryList>();
  m_stateMachine = fact.make(
      &DisplayedElementsToolPaletteFactory::make,
      *this,
      interval,
      &view().baseItem());

  m_updatingView = true;
  m_scenarioPresenter.on_displayedIntervalChanged(interval);
  m_updatingView = false;
  connect(
      m_scenarioPresenter.intervalPresenter(),
      &FullViewIntervalPresenter::intervalSelected,
      this,
      &ScenarioDocumentPresenter::setDisplayedInterval);

  on_viewReady();
  updateMinimap();
  view().view().verticalScrollBar()->setValue(0);

  view().timeRuler().setGrid(m_scenarioPresenter.intervalPresenter()->grid());
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

void ScenarioDocumentPresenter::setNewSelection(const Selection& old, const Selection& s)
{
  static QMetaObject::Connection cur_proc_connection;
  auto process = m_focusManager.focusedModel();

  for(auto& e : old)
  {
    const auto it = ossia::find(s, e);
    if(it == s.end())
    {
      if(auto proc = qobject_cast<const Process::ProcessModel*>(e))
      {
        proc->selection.set(false);
      }
      else if(auto port = qobject_cast<const Process::Port*>(e))
      {
        port->selection.set(false);
      }
      else if(auto cable = qobject_cast<const Process::Cable*>(e))
      {
        cable->selection.set(false);
      }
    }
  }

  // Manages the selection (different case if we're
  // selecting something in a process, or something in full view)
  if (s.empty())
  {
    if (process)
    {
      process->setSelection(Selection{});
      process->selection.set(false);
      QObject::disconnect(cur_proc_connection);
    }

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
    if (process)
    {
      process->setSelection(Selection{});
      process->selection.set(false);
      QObject::disconnect(cur_proc_connection);
    }

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
    auto newProc = Process::parentProcess(*s.begin());
    if (process && newProc != process)
    {
      process->setSelection(Selection{});
      process->selection.set(false);
      QObject::disconnect(cur_proc_connection);
    }

    if (newProc)
    {
      if (auto p = qobject_cast<const Process::ProcessModel*>(*s.begin()))
      {
        // the process itself is being selected
        p->selection.set(true);
      }
      else
      {
        newProc->setSelection(s);
        if (process)
        {
          process->selection.set(false);
        }
        newProc->selection.set(true);
      }
      cur_proc_connection = connect(
          newProc,
          &Process::ProcessModel::identified_object_destroying,
          this,
          [&] { m_selectionDispatcher.setAndCommit(Selection{}); },
          Qt::UniqueConnection);
    }

    for(auto& elt : s)
    {
      if(auto cable = qobject_cast<const Process::Cable*>(elt.data()))
        cable->selection.set(true);
      else if(auto port = qobject_cast<const Process::Port*>(elt.data()))
        port->selection.set(true);
    }
  }

  view().view().setFocus();
}

Process::ProcessFocusManager& ScenarioDocumentPresenter::focusManager() const
{
  return m_focusManager;
}
}

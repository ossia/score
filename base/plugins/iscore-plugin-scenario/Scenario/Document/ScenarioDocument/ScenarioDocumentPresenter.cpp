#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/FullView/FullViewConstraintPresenter.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactoryList.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Document/TimeRuler/MainTimeRuler/TimeRulerPresenter.hpp>
#include <Scenario/Document/TimeRuler/MainTimeRuler/TimeRulerView.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/tools/Clamp.hpp>
#include <iscore/widgets/DoubleSlider.hpp>

#include <Process/Style/ScenarioStyle.hpp>
#include <QPolygon>
#include <QSize>
#include <QString>
#include <QWidget>
#include <QtGlobal>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProviderList.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include "ScenarioDocumentPresenter.hpp"
#include "ZoomPolicy.hpp"
#include <Process/LayerPresenter.hpp>
#include <Process/TimeValue.hpp>
#include <Process/Tools/ProcessGraphicsView.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioScene.hpp>
#include <QMainWindow>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegatePresenter.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/statemachine/GraphicsSceneToolPalette.hpp>
#include <iscore/model/path/ObjectIdentifier.hpp>
#include <iscore/model/path/ObjectPath.hpp>
#include <iscore/tools/Todo.hpp>

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
    const iscore::DocumentContext& ctx,
    iscore::DocumentPresenter* parent_presenter,
    const iscore::DocumentDelegateModel& delegate_model,
    iscore::DocumentDelegateView& delegate_view)
    : DocumentDelegatePresenter{parent_presenter, delegate_model,
                                         delegate_view}
    , m_scenarioPresenter{new DisplayedElementsPresenter{this}}
    , m_selectionDispatcher{ctx.selectionStack}
    , m_mainTimeRuler{new TimeRulerPresenter{view().timeRuler(), this}}
    , m_focusManager{ctx.document.focusManager()}
    , m_context{ctx, m_focusDispatcher}

{
  using namespace iscore;

  // Setup the connections
  con(view(), &ScenarioDocumentView::horizontalZoomChanged, this,
      &ScenarioDocumentPresenter::on_zoomSliderChanged);

  con(iscore::GUIAppContext().mainWindow, SIGNAL(sizeChanged(QSize)),
      this, SLOT(on_windowSizeChanged(QSize)), Qt::QueuedConnection);
  con(view().view(), &ProcessGraphicsView::sizeChanged, this,
      &ScenarioDocumentPresenter::on_viewSizeChanged);
  con(view().view(), &ProcessGraphicsView::zoom, this,
      &ScenarioDocumentPresenter::on_zoomOnWheelEvent);
  con(view().view(), &ProcessGraphicsView::scrolled, this,
      &ScenarioDocumentPresenter::on_horizontalPositionChanged);

  connect(
      m_scenarioPresenter,
      &DisplayedElementsPresenter::requestFocusedPresenterChange,
      &focusManager(),
      static_cast<void (Process::ProcessFocusManager::*)(
          QPointer<Process::LayerPresenter>)>(
          &Process::ProcessFocusManager::focus));

  connect(
      m_mainTimeRuler->view(), &TimeRulerView::drag, this,
      [&](QPointF prev, QPointF current) {
        on_timeRulerScrollEvent(prev, current);
      });

  // Focus
  connect(
      &m_focusDispatcher, SIGNAL(focus(QPointer<Process::LayerPresenter>)),
      this, SIGNAL(setFocusedPresenter(QPointer<Process::LayerPresenter>)),
      Qt::QueuedConnection);

  con(ctx.app.settings<Settings::Model>(),
      &Settings::Model::GraphicZoomChanged, this, [&](double d) {
        auto& skin = ScenarioStyle::instance();
        skin.setConstraintWidth(d);
      });


  // Help for the FocusDispatcher.
  connect(
      this, &ScenarioDocumentPresenter::setFocusedPresenter, &m_focusManager,
      static_cast<void (Process::ProcessFocusManager::*)(
          QPointer<Process::LayerPresenter>)>(
          &Process::ProcessFocusManager::focus));

  con(m_focusManager, &Process::ProcessFocusManager::sig_defocusedViewModel,
      this, &ScenarioDocumentPresenter::on_viewModelDefocused);
  con(m_focusManager, &Process::ProcessFocusManager::sig_focusedViewModel,
      this, &ScenarioDocumentPresenter::on_viewModelFocused);
  con(m_focusManager, &Process::ProcessFocusManager::sig_focusedRoot,
      this, [] {
    ScenarioApplicationPlugin& app = iscore::GUIAppContext().guiApplicationPlugin<ScenarioApplicationPlugin>();
    app.editionSettings().setExpandMode(ExpandMode::GrowShrink);
  }, Qt::QueuedConnection);

  setDisplayedConstraint(model().baseConstraint());
}

ScenarioDocumentPresenter::~ScenarioDocumentPresenter()
{
  delete m_scenarioPresenter;
}

ConstraintModel& ScenarioDocumentPresenter::displayedConstraint() const
{
  return displayedElements.constraint();
}

const DisplayedElementsPresenter&ScenarioDocumentPresenter::presenters() const
{
  return *m_scenarioPresenter;
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
  iscore::SelectionDispatcher{m_context.selectionStack}
    .setAndCommit(
     {&displayedElements.startState(),
      &displayedElements.constraint(),
      &displayedElements.endState()});
}

void ScenarioDocumentPresenter::on_displayedConstraintChanged()
{
  auto& gv = view().view();
  auto& cst = displayedConstraint();
  // Setup of the state machine.
  auto& ctx = iscore::IDocument::documentContext(model());
  const auto& fact
      = ctx.app.interfaces<DisplayedElementsToolPaletteFactoryList>();
  m_stateMachine
      = fact.make(&DisplayedElementsToolPaletteFactory::make, *this, cst);
  m_scenarioPresenter->on_displayedConstraintChanged(cst);
  connect(
      m_scenarioPresenter->constraintPresenter(),
      &FullViewConstraintPresenter::constraintSelected, this,
      &ScenarioDocumentPresenter::setDisplayedConstraint);

  // Set a new zoom ratio, such that the displayed constraint takes the whole
  // screen.

  double newZoom = displayedConstraint().zoom();
  auto rect = displayedConstraint().visibleRect();

  if (newZoom != -1) // constraint has already been in fullview
  {
    view().zoomSlider()->setValue(newZoom);
    newZoom = ZoomPolicy::sliderPosToZoomRatio(
        0.01,
        displayedDuration(),
        view().viewWidth());
  }
  else // first time in fullview : init the zoom ratio
  {
    view().zoomSlider()->setValue(0.01);
    newZoom = ZoomPolicy::sliderPosToZoomRatio(
        0.01,
        displayedDuration(),
        view().viewWidth());
  }

  setMillisPerPixel(newZoom);

  // scroll to the last center position
  gv.ensureVisible(
      gv.mapFromScene(rect)
          .boundingRect());
}

void ScenarioDocumentPresenter::setMillisPerPixel(ZoomRatio newRatio)
{
  m_zoomRatio = newRatio;

  m_mainTimeRuler->setPixelPerMillis(1.0 / m_zoomRatio);
  m_scenarioPresenter->on_zoomRatioChanged(m_zoomRatio);
}

void ScenarioDocumentPresenter::on_zoomSliderChanged(double sliderPos)
{
  auto newMillisPerPix = ZoomPolicy::sliderPosToZoomRatio(
      sliderPos,
      displayedDuration(),
      view().viewWidth());

  updateZoom(newMillisPerPix, QPointF{}); // Null point
}

void ScenarioDocumentPresenter::on_zoomOnWheelEvent(
    QPointF zoom, QPointF center)
{
  // convert the mouse displacement into a fake slider move

  auto& slider = *view().zoomSlider();
  double zoomSpeed = 1.5; // experiment value
  double newSliderPos
      = (slider.value()
         + zoomSpeed * double(zoom.y())
               / (double(slider.width() * (1. + slider.value()))));

  newSliderPos = clamp(newSliderPos, 0., 1.);

  slider.setValue(newSliderPos);

  auto newMillisPerPix = ZoomPolicy::sliderPosToZoomRatio(
      newSliderPos,
      displayedDuration(),
      view().viewWidth());

  updateZoom(newMillisPerPix, center);
}

void ScenarioDocumentPresenter::on_timeRulerScrollEvent(
    QPointF previous, QPointF current)
{
  view().view().scrollHorizontal(previous.x() - current.x());
}

void ScenarioDocumentPresenter::on_windowSizeChanged(QSize)
{
  auto& gv = view().view();
  auto zoom = ZoomPolicy::sliderPosToZoomRatio(
      view().zoomSlider()->value(),
      displayedDuration(),
      view().viewWidth());

  updateZoom(zoom, {0, 0});

  // update the center of view
  displayedConstraint().setVisibleRect(
      gv.mapToScene(gv.viewport()->rect()).boundingRect());
}

void ScenarioDocumentPresenter::on_viewSizeChanged(QSize s)
{
  m_mainTimeRuler->view()->setWidth(s.width());
}

void ScenarioDocumentPresenter::on_horizontalPositionChanged(int dx)
{
  auto& c = displayedConstraint();
  auto& gv = view().view();

  if(dx < 0 && !m_zooming)
  {
    auto cur_rect = gv.mapToScene(gv.rect()).boundingRect();
    auto scene_rect = gv.sceneRect();
    if(cur_rect.x() + cur_rect.width() - dx > (scene_rect.width()))
    {
      c.duration.setGuiDuration(TimeVal::fromMsecs(m_zoomRatio * (cur_rect.x() + cur_rect.width() - dx)));
      scene_rect.adjust(0, 0, 5, 0);
      gv.setSceneRect(scene_rect);
    }
  }
  else if(dx > 0 && !m_zooming)
  {
    TimeVal min_time = (c.duration.isMaxInfinite() ? c.duration.defaultDuration() : c.duration.maxDuration()) * 1.1;
    for(Process::ProcessModel& proc : c.processes)
    {
      if(proc.contentHasDuration())
      {
        auto d = proc.contentDuration();
        if(d > min_time)
          min_time = d;
      }
    }
    if(min_time < c.duration.guiDuration())
    {
      auto cur_rect = gv.mapToScene(gv.rect()).boundingRect();
      c.duration.setGuiDuration(std::max(TimeVal::fromMsecs(m_zoomRatio * (cur_rect.x() + cur_rect.width() - dx)), min_time));
      auto scene_rect = gv.sceneRect();
      scene_rect.adjust(0, 0, -dx, 0);
      gv.setSceneRect(scene_rect);
    }
  }

  QRect viewport_rect = gv.viewport()->rect();
  QRectF visible_scene_rect = gv.mapToScene(viewport_rect).boundingRect();

  m_mainTimeRuler->setStartPoint(
      TimeVal::fromMsecs(visible_scene_rect.x() * m_zoomRatio));
  displayedConstraint().setVisibleRect(visible_scene_rect);
}

void ScenarioDocumentPresenter::updateRect(const QRectF& rect)
{
  view().view().setSceneRect(rect);
}

const Process::ProcessPresenterContext&ScenarioDocumentPresenter::context() const
{
  return m_context;
}

void ScenarioDocumentPresenter::updateZoom(ZoomRatio newZoom, QPointF focus)
{
  m_zooming = true;
  auto& gv = view().view();
  auto& vp = *gv.viewport();
  auto w = vp.width();
  auto h = vp.height();

  QRect viewport_rect = vp.rect();
  QRectF visible_scene_rect = gv.mapToScene(viewport_rect).boundingRect();

  qreal center = focus.x();
  if (focus.isNull())
  {
    center = visible_scene_rect.center().x();
  }
  else if (focus.x() - visible_scene_rect.left() < 40)
  {
    center = visible_scene_rect.left();
  }
  else if (visible_scene_rect.right() - focus.x() < 40)
  {
    center = visible_scene_rect.right();
  }

  qreal centerT = center * m_zoomRatio; // here's the old zoom

  auto deltaX = center - visible_scene_rect.left();

  auto y = visible_scene_rect.top();

  if (newZoom != m_zoomRatio)
    setMillisPerPixel(newZoom);

  qreal x = centerT / m_zoomRatio - deltaX;
  ; // here's the new zoom

  auto newView = QRectF{x, y, (qreal)w, (qreal)h};

  gv.ensureVisible(newView, 0, 0);

  QRectF new_visible_scene_rect = gv.mapToScene(vp.rect()).boundingRect();

  m_mainTimeRuler->view()->setWidth(gv.width());
  // TODO should call displayedElementsPresenter instead??
  displayedConstraint().setZoom(view().zoomSlider()->value());
  displayedConstraint().setVisibleRect(new_visible_scene_rect);
  m_zooming = false;
}

double ScenarioDocumentPresenter::displayedDuration() const
{
  return 0.9 * displayedConstraint().duration.guiDuration().msec();
}

void ScenarioDocumentPresenter::setDisplayedConstraint(ConstraintModel& constraint)
{
  if (displayedElements.initialized())
  {
    if (&constraint == &displayedElements.constraint())
    {
      auto pres = iscore::IDocument::try_get<ScenarioDocumentPresenter>(
            *iscore::IDocument::documentFromObject(this));
      if(pres) pres->selectTop();
      return;
    }
  }

  auto& provider
      = iscore::IDocument::documentContext(*this)
            .app.interfaces<DisplayedElementsProviderList>();
  displayedElements.setDisplayedElements(
      provider.make(&DisplayedElementsProvider::make, constraint));

  m_focusManager.focusNothing();

  disconnect(m_constraintConnection);
  if (&constraint != &model().baseConstraint())
  {
    m_constraintConnection
        = con(constraint, &QObject::destroyed, this, [&]() {
            setDisplayedConstraint(model().baseConstraint());
          });
  }

  on_displayedConstraintChanged();
}


void ScenarioDocumentPresenter::on_viewModelDefocused(
    const Process::ProcessModel* vm)
{
  // Deselect
  // Note : why these two lines ?
  // selectionStack.clear() should clear the selection everywhere anyway.
  if (vm)
    vm->setSelection({});

  iscore::IDocument::documentContext(*this).selectionStack.clearAllButLast();
}

void ScenarioDocumentPresenter::on_viewModelFocused(
    const Process::ProcessModel* process)
{
  // If the parent of the layer is a constraint, we set the focus on the
  // constraint too.
  auto slot = process->parent();
  if (!slot)
    return;
  auto rack = slot->parent();
  if (!rack)
    return;
  auto cm = rack->parent();
  if (auto constraint = dynamic_cast<ConstraintModel*>(cm))
  {
    if (m_focusedConstraint)
      m_focusedConstraint->focusChanged(false);

    m_focusedConstraint = constraint;
    m_focusedConstraint->focusChanged(true);
  }
}

void ScenarioDocumentPresenter::updateMaxWidth(double w)
{
  // TODO save it in the constraint.
  // By default -1.
  // Then when displaying, have a "get longest duration" which returns
  // the correction duration between default, max, maxGraphical / maxContent
}

void ScenarioDocumentPresenter::setNewSelection(const Selection& s)
{
  auto process = m_focusManager.focusedModel();

  // Manages the selection (different case if we're
  // selecting something in a process, or something in full view)
  if (s.empty())
  {
    if (process)
    {
      process->setSelection(Selection{});
    }

    displayedElements.setSelection(Selection{});
    // Note : once here was a call to defocus a presenter. Why ? See git blame.
  }
  else if (ossia::any_of(s, [&](const QObject* obj) {
             return obj == &displayedElements.constraint()
                    || obj == &displayedElements.startTimeNode()
                    || obj == &displayedElements.endTimeNode()
                    || obj == &displayedElements.startEvent()
                    || obj == &displayedElements.endEvent()
                    || obj == &displayedElements.startState()
                    || obj == &displayedElements.endState();
           }))
  {
    if (process)
    {
      process->setSelection(Selection{});
    }

    m_focusManager.focus(
          &iscore::IDocument::get<Scenario::ScenarioDocumentPresenter>(*iscore::IDocument::documentFromObject(this)));

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
    }

    if (newProc)
    {
      newProc->setSelection(s);
    }
  }

  view().view().setFocus();
}

Process::ProcessFocusManager&ScenarioDocumentPresenter::focusManager() const
{
  return m_focusManager;
}
}

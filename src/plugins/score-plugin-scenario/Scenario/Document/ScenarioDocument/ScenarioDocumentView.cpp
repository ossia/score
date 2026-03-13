// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioDocumentView.hpp"

#include <Process/Dataflow/CableItem.hpp>
#include <Process/Dataflow/PortItem.hpp>

#include <Scenario/Application/Menus/TransportActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalView.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <Scenario/Document/ScenarioDocument/SnapshotAction.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/graphics/BackgroundRenderer.hpp>
#include <score/graphics/GraphicsProxyObject.hpp>
#include <score/model/Skin.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/TextLabel.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QOpenGLWindow>
#include <QPainter>
#include <QRect>
#include <QScrollBar>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

#include <Transport/DocumentPlugin.hpp>

#if defined(SCORE_WEBSOCKETS)
#include "WebSocketView.hpp"
#endif
#include <Process/Style/ScenarioStyle.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::ScenarioDocumentView)
W_OBJECT_IMPL(Scenario::ProcessGraphicsView)
namespace Scenario
{

namespace
{
static int defaultEditorRefreshRate()
{
  const auto tcount = QThread::idealThreadCount();
  if(tcount <= 4)
    return 30;
  else if(tcount <= 8)
    return 16;
  else
    return 8;
}
}
ProcessGraphicsView::ProcessGraphicsView(
    const score::GUIApplicationContext& ctx, QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView{scene, parent}
    , m_app{ctx}
    , m_opengl{ctx.applicationSettings.opengl}
{
  m_lastwheel = std::chrono::steady_clock::now();

  setAlignment(Qt::AlignTop | Qt::AlignLeft);
  setFrameStyle(0);
  setDragMode(QGraphicsView::NoDrag);
  setAcceptDrops(true);
  setFocusPolicy(Qt::WheelFocus);
  setRenderHints(
      QPainter::Antialiasing | QPainter::SmoothPixmapTransform
      | QPainter::TextAntialiasing);
  // setCacheMode(QGraphicsView::CacheBackground);

#if !defined(__EMSCRIPTEN__)
  setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
  setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
  setAttribute(Qt::WA_PaintOnScreen, false);
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  viewport()->setAttribute(Qt::WA_OpaquePaintEvent, true);
#endif
  /*startTimer(32);

#if defined(__APPLE__)
  // setRenderHints(0);
  // setOptimizationFlag(QGraphicsView::IndirectPainting, true);
#endif*/
}

ProcessGraphicsView::~ProcessGraphicsView() { }

void ProcessGraphicsView::scrollHorizontal(double dx)
{
  if(auto bar = horizontalScrollBar())
  {
    bar->setValue(bar->value() + dx);
    if(m_opengl)
      viewport()->update();
  }
}

QRectF ProcessGraphicsView::visibleRect() const noexcept
{
  return QRectF{
      this->mapToScene(QPoint{}), this->mapToScene(this->rect().bottomRight())};
}

void ProcessGraphicsView::drawForeground(QPainter* painter, const QRectF& rect)
{
  if(timebarVisible && currentView && currentTimebar)
  {
    auto pctg = currentTimebar->playPercentage();

    auto x = pctg * currentView->defaultWidth();
    double view_x = currentView->mapToScene(x, 0.).x();

    if(x < rect.x() || x > rect.x() + rect.width())
      return;

    auto top = mapToScene(QPoint{0, 0}).y();
    auto bottom = mapToScene(QPoint{0, height()}).y();

    static const QPen pen(QBrush(Qt::gray), 1.0);

    painter->setPen(pen);
    painter->drawLine(QPointF{view_x - 0.5, top}, QPointF{view_x - 0.5, bottom});
  }
}

void ProcessGraphicsView::drawBackground(QPainter* painter, const QRectF& rect)
{
  if(!this->currentBackground)
  {
    if(this->m_globalRenderers.empty())
    {
      return QGraphicsView::drawBackground(painter, rect);
    }
    else
    {
      if(this->m_globalRenderers.back()->render(painter, rect))
        return;
      else
        return QGraphicsView::drawBackground(painter, rect);
    }
  }

  if(currentBackground->render(painter, rect))
    return;
  else
    return QGraphicsView::drawBackground(painter, rect);
}

void ProcessGraphicsView::resizeEvent(QResizeEvent* ev)
{
  QGraphicsView::resizeEvent(ev);
  sizeChanged(size());

  visibleRectChanged(visibleRect());

  if(m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::scrollContentsBy(int dx, int dy)
{
  QGraphicsView::scrollContentsBy(dx, dy);

  this->scene()->update();
  if(dx != 0)
    scrolled(dx);

  visibleRectChanged(visibleRect());

  if(m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::wheelEvent(QWheelEvent* event)
{
  {
    // Code from QGraphicsView::wheelEvent ; we want to propagate to children
    // but we handle scrolling ourselves (slower, Qt scrolling is too fast)
    QGraphicsSceneWheelEvent wheelEvent(QEvent::GraphicsSceneWheel);
    wheelEvent.setWidget(viewport());
    wheelEvent.setScenePos(mapToScene(event->position().toPoint()));
    wheelEvent.setScreenPos(event->globalPosition().toPoint());
    wheelEvent.setButtons(event->buttons());
    wheelEvent.setModifiers(event->modifiers());
    const bool horizontal
        = qAbs(event->angleDelta().x()) > qAbs(event->angleDelta().y());
    wheelEvent.setDelta(horizontal ? event->angleDelta().x() : event->angleDelta().y());
    wheelEvent.setPixelDelta(event->pixelDelta());
    wheelEvent.setPhase(event->phase());
    wheelEvent.setInverted(event->isInverted());
    wheelEvent.setOrientation(horizontal ? Qt::Horizontal : Qt::Vertical);
    wheelEvent.setAccepted(false);
    wheelEvent.setTimestamp(event->timestamp());
    QCoreApplication::sendEvent(this->scene(), &wheelEvent);
  }

  const auto pressedModifier = qApp->keyboardModifiers();
  const auto m_hZoom = pressedModifier == Qt::ControlModifier;
  const auto m_vZoom = pressedModifier == Qt::ShiftModifier;

#if !defined(__APPLE__)
  const auto t = std::chrono::steady_clock::now();
  static int wheelCount = 0;

  if(std::chrono::duration_cast<std::chrono::milliseconds>(t - m_lastwheel).count() < 16
     && wheelCount < 3)
  {
    wheelCount++;
    return;
  }
  wheelCount = 0;
  m_lastwheel = t;
#endif
  const QPoint angleDelta = event->angleDelta();
  const QPointF delta = {angleDelta.x() / 8., angleDelta.y() / 8.};
  if(m_hZoom)
  {
    QPoint pos = event->position().toPoint();
    horizontalZoom(delta, mapToScene(pos));
    return;
  }
  else if(m_vZoom)
  {
    QPoint pos = event->position().toPoint();
    verticalZoom(delta, mapToScene(pos));
    return;
  }

  const auto pixDelta = event->pixelDelta();

  const auto hsb = this->horizontalScrollBar();
  const auto vsb = this->verticalScrollBar();

  if(pixDelta != QPoint{})
  {
    hsb->setValue(hsb->value() - event->pixelDelta().x() / 2.);
    vsb->setValue(vsb->value() - event->pixelDelta().y() / 2.);
  }
  else if(angleDelta != QPoint{})
  {
    struct MyWheelEvent : public QWheelEvent
    {
      MyWheelEvent(const QWheelEvent& other)
          : QWheelEvent{other}
      {
        this->m_angleDelta.ry() /= 4.;
        this->m_pixelDelta.ry() /= 4.;
      }
    };
    MyWheelEvent e{*event};

    if(qAbs(event->angleDelta().x()) > qAbs(event->angleDelta().y()))
      QCoreApplication::sendEvent(hsb, &e);
    else
      QCoreApplication::sendEvent(vsb, &e);
  }

  if(m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::keyPressEvent(QKeyEvent* event)
{
  keyPress(event);
  for(auto& plug : m_app.guiApplicationPlugins())
    plug->on_keyPressEvent(*event);
  event->ignore();

  QGraphicsView::keyPressEvent(event);

  if(m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::keyReleaseEvent(QKeyEvent* event)
{
  keyRelease(event);
  for(auto& plug : m_app.guiApplicationPlugins())
    plug->on_keyReleaseEvent(*event);
  event->ignore();

  QGraphicsView::keyReleaseEvent(event);

  if(m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::focusOutEvent(QFocusEvent* event)
{
  focusedOut();
  event->ignore();

  QGraphicsView::focusOutEvent(event);

  if(m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::leaveEvent(QEvent* event)
{
  focusedOut();
  QGraphicsView::leaveEvent(event);

  if(m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::checkAndRemoveCurrentDialog(QPoint pos)
{
  // Close the small output panels (gain/pan, etc) if we're clicking somewhere else
  if(auto dialog = this->scene()->activePanel())
  {
    const auto notChildOfDialog = [dialog](QGraphicsItem* item) {
      if(!item)
        return true;

      auto parent = item->parentItem();
      if(!parent || (parent != dialog && parent->parentItem() != dialog))
        return true;

      return false;
    };
    const auto other = itemAt(pos);
    if(other)
    {
      switch(other->type())
      {
        case Dataflow::PortItem::Type: {
          if(notChildOfDialog(other))
          {
            delete dialog;
          }
          break;
        }
        case Dataflow::CableItem::Type: {
          auto cable = static_cast<Dataflow::CableItem*>(other);
          if(notChildOfDialog(cable->source()) && notChildOfDialog(cable->target()))
          {
            delete dialog;
          }
          break;
        }
        default: {
          const auto mapped_pos = other->mapToItem(dialog, QPointF{0, 0});
          if(!dialog->contains(mapped_pos))
          {
            delete dialog;
          }
          break;
        }
      }
    }
    else
    {
      delete dialog;
    }
  }
}

void ProcessGraphicsView::mousePressEvent(QMouseEvent* event)
{
  mousePress(event);
  // workaround for some items not getting release event sometimes.
  if(m_press_release_chain.contains(event->button()))
  {
    QMouseEvent ev(
        QEvent::Type::MouseButtonRelease, event->pos(), event->globalPosition(),
        event->button(), event->button() | event->buttons(), Qt::KeyboardModifiers{});

    QGraphicsView::mouseReleaseEvent(&ev);
    m_press_release_chain.erase(event->button());
  }
  m_press_release_chain.insert(event->button());
  checkAndRemoveCurrentDialog(event->pos());

  QGraphicsView::mousePressEvent(event);

  // Handle right-click menu in nodal view
  if(event->button() & Qt::RightButton)
  {
    const auto item = itemAt(event->pos());
    if(!item)
    {
      emptyContextMenuRequested(event->pos());
    }
  }

  if(m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::mouseMoveEvent(QMouseEvent* event)
{
  mouseMove(event);
  QGraphicsView::mouseMoveEvent(event);

  if(m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::mouseReleaseEvent(QMouseEvent* event)
{
  mouseRelease(event);
  QGraphicsView::mouseReleaseEvent(event);
  if(m_opengl)
    viewport()->update();
  m_press_release_chain.erase(event->button());
}

void ProcessGraphicsView::dragEnterEvent(QDragEnterEvent* event)
{
  QGraphicsView::dragEnterEvent(event);
  event->accept();
}

void ProcessGraphicsView::dragMoveEvent(QDragMoveEvent* event)
{
  QGraphicsView::dragMoveEvent(event);
  event->accept();
}

void ProcessGraphicsView::dragLeaveEvent(QDragLeaveEvent* event)
{
  QGraphicsView::dragLeaveEvent(event);
  event->accept();
}

void ProcessGraphicsView::dropEvent(QDropEvent* event)
{
  auto item = itemAt(event->position().toPoint());
  if(!item)
  {
    dropRequested(event->position().toPoint(), event->mimeData());
    event->accept();
  }
  else if(auto t = item->type(); t >= 90076 && t <= 90078)
  {
    // Drop on timebars in full view
    dropRequested(event->position().toPoint(), event->mimeData());
    event->accept();
  }
  else
  {
    QGraphicsView::dropEvent(event);
  }
  dropFinished();
}

void ProcessGraphicsView::contextMenuEvent(QContextMenuEvent* event)
{
  // We check the cursor in order to prevent some amount of buggy cases of editing
  // a slider / knob and falling into https://bugreports.qt.io/projects/QTBUG/issues/QTBUG-97044
  auto cur = QGuiApplication::overrideCursor();
  if(cur)
  {
    if(cur->shape() == Qt::BlankCursor)
    {
      event->accept();
      return;
    }
  }

  QGraphicsView::contextMenuEvent(event);
}

bool ProcessGraphicsView::event(QEvent* event)
{
  switch(event->type())
  {
    case QEvent::HoverEnter:
      hoverEnterEvent(static_cast<QHoverEvent*>(event));
      return QGraphicsView::event(event);
    case QEvent::HoverLeave:
      hoverLeaveEvent(static_cast<QHoverEvent*>(event));
      return QGraphicsView::event(event);
    case QEvent::HoverMove:
      hoverMoveEvent(static_cast<QHoverEvent*>(event));
      return QGraphicsView::event(event);
    // case QEvent::TabletPress:
    // case QEvent::TabletRelease:
    case QEvent::TabletMove:
      tabletMove(static_cast<QTabletEvent*>(event));
      return QGraphicsView::event(event);

#if !defined(QT_NO_GESTURES)
    case QEvent::NativeGesture: {
      auto gest = static_cast<QNativeGestureEvent*>(event);
      switch(gest->gestureType())
      {
        case Qt::NativeGestureType::ZoomNativeGesture: {
          double zoom = gest->value() * 100.;
          QPointF delta = {zoom, zoom};

          QPointF pos = this->mapFromGlobal(gest->globalPosition());
          horizontalZoom(delta, mapToScene(pos.toPoint()));

          return true;
        }
        default:
          return QGraphicsView::event(event);
      }

      return true;
    }
#endif

    default:
      return QGraphicsView::event(event);
  }
}

void ProcessGraphicsView::hoverEnterEvent(QHoverEvent* event)
{
  hoverEnter(event);
}

void ProcessGraphicsView::hoverMoveEvent(QHoverEvent* event)
{
  hoverMove(event);
  const auto scenePos = this->mapToScene(event->position().toPoint());
  auto items = this->scene()->items(scenePos);
  auto set_tip = [&](const QString& t) {
    QStatusTipEvent ev{t};
    auto obj = reinterpret_cast<QObject*>(this->m_app.mainWindow);
    obj->event(&ev);
  };
  for(int i = 0; i < items.size(); ++i)
  {
    if(const auto& tooltip = items.at(i)->toolTip(); !tooltip.isEmpty())
    {
      set_tip(tooltip);
      return;
    }
  }

  set_tip(QString{});
}
void ProcessGraphicsView::hoverLeaveEvent(QHoverEvent* event)
{
  hoverLeave(event);
}

ScenarioDocumentView::ScenarioDocumentView(
    const score::DocumentContext& ctx, QObject* parent)
    : score::DocumentDelegateView{parent}
    , m_widget{new QWidget}
    , m_context{ctx}
    , m_scene{m_widget}
    , m_view{ctx.app, &m_scene, m_widget}
    , m_timeRulerView{&m_timeRulerScene}
    , m_timeRuler{new MusicalRuler{&m_timeRulerView}}
    , m_minimapScene{m_widget}
    , m_minimapView{&m_minimapScene}
    , m_minimap{&m_minimapView}
{
  auto& scenario_settings = ctx.app.settings<Scenario::Settings::Model>();

  con(ctx.document.commandStack(), &score::CommandStack::stackChanged, this,
      [&] { m_view.viewport()->update(); });

#if defined(SCORE_WEBSOCKETS)
  auto wsview = new WebSocketView(m_scene, 9998, this);
#endif

  score::setHelp(&m_view, "Main score view. Drop things in here.");
  score::setHelp(
      &m_timeRulerView, "The time ruler keeps track of time. Scroll by dragging it.");
  score::setHelp(
      &m_minimapView, "A minimap which shows an overview of the topmost score");
  m_view.setSizePolicy(QSizePolicy{QSizePolicy::Expanding, QSizePolicy::Expanding});

  m_widget->addAction(new SnapshotAction{m_scene, m_widget});

  // Transport
  /// Zoom
  QAction* zoomIn = new QAction(tr("Zoom in"), m_widget);
  m_widget->addAction(zoomIn);
  zoomIn->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  zoomIn->setShortcuts({QKeySequence::ZoomIn, tr("Ctrl+=")});
  connect(zoomIn, &QAction::triggered, this, [&] { m_minimap.zoomIn(); });
  QAction* zoomOut = new QAction(tr("Zoom out"), m_widget);
  m_widget->addAction(zoomOut);
  zoomOut->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  zoomOut->setShortcut(QKeySequence::ZoomOut);
  connect(zoomOut, &QAction::triggered, this, [&] { m_minimap.zoomOut(); });
  QAction* largeView = new QAction{tr("Large view"), m_widget};
  m_widget->addAction(largeView);
  largeView->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  largeView->setShortcut(tr("Ctrl+0"));
  connect(
      largeView, &QAction::triggered, this, [this] { setLargeView(); },
      Qt::QueuedConnection);
  con(m_minimap, &Minimap::rescale, largeView, &QAction::trigger);

  // Time Ruler

  {
    auto setupTimeRuler = [this, largeView](bool b) {
      delete m_timeRuler;
      if(b)
        m_timeRuler = new MusicalRuler{&m_timeRulerView};
      else
        m_timeRuler = new TimeRuler{&m_timeRulerView};

      connect(m_timeRuler, &TimeRuler::rescale, largeView, &QAction::trigger);
      connect(
          m_timeRuler, &TimeRuler::doubleClicked, this,
          &ScenarioDocumentView::requestTransport);
      m_timeRulerScene.addItem(m_timeRuler);
      timeRulerChanged();
    };

    con(scenario_settings, &Settings::Model::MeasureBarsChanged, this,
        [setupTimeRuler](bool b) { setupTimeRuler(b); });
    setupTimeRuler(scenario_settings.getMeasureBars());
  }

  // view layout
  m_scene.addItem(&m_baseObject);

  auto lay = new score::MarginLess<QVBoxLayout>;
  m_widget->setLayout(lay);
  m_widget->setContentsMargins(0, 0, 0, 0);

  m_minimapScene.addItem(&m_minimap);
  m_minimapScene.setItemIndexMethod(QGraphicsScene::NoIndex);

  m_timeRulerScene.setItemIndexMethod(QGraphicsScene::NoIndex);

  lay->addWidget(&m_minimapView);
  lay->addWidget(&m_timeRulerView);
  lay->addWidget(&m_view);

  lay->setSpacing(1);

  m_view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  m_view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  auto& skin = score::Skin::instance();
  con(skin, &score::Skin::changed, this, [&] {
    auto& skin = Process::Style::instance();
    m_timeRulerView.setBackgroundBrush(skin.MinimapBackground());
    m_minimapView.setBackgroundBrush(skin.MinimapBackground());
    m_view.setBackgroundBrush(skin.Background());
  });

  skin.changed();

  m_widget->setObjectName("ScenarioViewer");

  // Cursors
  con(this->view(), &ProcessGraphicsView::focusedOut, this, [&] {
    auto& es
        = ctx.app.guiApplicationPlugin<ScenarioApplicationPlugin>().editionSettings();
    es.setTool(Scenario::Tool::Select);
  });

  updateBackgroundMode();

  // Background
  {
    auto& es
        = ctx.app.guiApplicationPlugin<ScenarioApplicationPlugin>().editionSettings();
    connect(
        &es, &EditionSettings::viewSelection, this,
        &ScenarioDocumentView::on_viewSelection);
  }
}

void ScenarioDocumentView::updateBackgroundMode()
{
  if(m_timer != -1)
  {
    killTimer(m_timer);
    m_timer = -1;
  }

  const bool opengl = m_context.app.applicationSettings.opengl;
  bool wantsFullUpdates = false;

  // Necessary to redraw the exec bar correctly...
  // On non-retina macOS, FullViewportupdate on software is faster than GLWidget -=-
  // Maybe different on Retina, it has to be checked...
  if(view().timebarVisible && view().timebarPlaying && !opengl)
    wantsFullUpdates = true;

  else if(!m_view.m_globalRenderers.empty())
    wantsFullUpdates = true;

  double refreshRate = defaultEditorRefreshRate();
  if(m_transport)
  {
    auto& scenario_settings = m_context.app.settings<Scenario::Settings::Model>();
    const auto rate = scenario_settings.getExecutionRefreshRate();
    if(rate > 0)
      refreshRate = 1000. / rate;
  }

  if(!wantsFullUpdates)
  {
    if(opengl)
    {
      // m_minimapView.setViewport(new QOpenGLWidget);
      // m_timeRulerView.setViewport(new QOpenGLWidget);
      auto vp = new QOpenGLWidget{};
      m_view.setViewport(vp);

      // m_minimapView.setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
      // m_timeRulerView.setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
      m_view.setViewportUpdateMode(QGraphicsView::NoViewportUpdate);

      // m_minimapView.viewport()->setUpdatesEnabled(true);
      // m_timeRulerView.viewport()->setUpdatesEnabled(true);
      m_view.viewport()->setUpdatesEnabled(true);

      m_timer = startTimer(refreshRate, Qt::PreciseTimer);
    }
    else
    {
      // m_minimapView.setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
      m_view.setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
      // m_timeRulerView.setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
    }
  }
  else
  {
    m_view.setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    m_timer = startTimer(refreshRate, Qt::PreciseTimer);
  }
  m_view.update();
}

ScenarioDocumentView::~ScenarioDocumentView() { }

void ScenarioDocumentView::zoom(double zx, double zy)
{
  if(zx > 0)
    m_minimap.zoomIn();
  else
    m_minimap.zoomOut();
}

void ScenarioDocumentView::scroll(double dx, double dy)
{
  auto& gv = this->m_view;
  if(dx != 0)
  {
    const auto hsb = gv.horizontalScrollBar();
    hsb->setValue(hsb->value() - dx);
  }
  if(dy != 0)
  {
    const auto vsb = gv.verticalScrollBar();

    vsb->setValue(vsb->value() - dy);
  }
}

void ScenarioDocumentView::addBackgroundRenderer(score::BackgroundRenderer* r)
{
  m_view.m_globalRenderers.push_back(r);
  updateBackgroundMode();
}

void ScenarioDocumentView::removeBackgroundRenderer(score::BackgroundRenderer* r)
{
  ossia::remove_erase(m_view.m_globalRenderers, r);
  updateBackgroundMode();
}

QWidget* ScenarioDocumentView::getWidget()
{
  return m_widget;
}

qreal ScenarioDocumentView::viewWidth() const
{
  return m_view.width();
}

QRectF ScenarioDocumentView::viewportRect() const
{
  return m_view.viewport()->rect();
}

QRectF ScenarioDocumentView::visibleSceneRect() const
{
  const auto viewRect = m_view.viewport()->rect();
  return QRectF{
      m_view.mapToScene(viewRect.topLeft()), m_view.mapToScene(viewRect.bottomRight())};
}

void ScenarioDocumentView::showRulers(bool b)
{
  m_minimapView.setVisible(b);
  m_timeRulerView.setVisible(b);
}

void ScenarioDocumentView::ready()
{
  const bool opengl = m_context.app.applicationSettings.opengl;

  if(opengl)
  {
    auto& transport = m_context.plugin<Transport::DocumentPlugin>();
    con(transport, &Transport::DocumentPlugin::play, this, [this] {
      m_transport = true;
      updateBackgroundMode();
    });

    con(transport, &Transport::DocumentPlugin::stop, this, [this] {
      m_transport = false;
      updateBackgroundMode();
    });
  }
}

void ScenarioDocumentView::timerEvent(QTimerEvent* event)
{
  // m_minimapView.viewport()->update();
  // m_timeRulerView.viewport()->update();
  m_view.viewport()->update();
}

void ScenarioDocumentView::on_viewSelection()
{
  if(m_view.currentBackground)
  {
    delete m_view.currentBackground;
    m_view.currentBackground = nullptr;
  }

  const auto& sel = this->m_context.selectionStack.currentSelection();
  if(sel.empty())
    return;

  auto& background_renderers
      = this->m_context.app.interfaces<score::BackgroundRendererList>();
  if(auto renderer
     = background_renderers.make(&score::BackgroundRendererFactory::make, sel, this))
  {
    m_view.currentBackground = renderer;
    qDebug() << renderer;
    connect(renderer, &score::BackgroundRenderer::destroyed, this, [this] {
      this->m_view.currentBackground = nullptr;
    });
  }
}
}

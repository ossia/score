// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioDocumentView.hpp"

#include <Process/Dataflow/CableItem.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Scenario/Application/Menus/TransportActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <Scenario/Document/ScenarioDocument/SnapshotAction.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/graphics/GraphicsProxyObject.hpp>
#include <score/model/Skin.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/TextLabel.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QAction>
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

#if defined(SCORE_WEBSOCKETS)
#include "WebSocketView.hpp"
#endif
#include <Process/Style/ScenarioStyle.hpp>

#include <QGLWidget>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::ScenarioDocumentView)
W_OBJECT_IMPL(Scenario::ProcessGraphicsView)
namespace Scenario
{

ProcessGraphicsView::ProcessGraphicsView(
    const score::GUIApplicationContext& ctx,
    QGraphicsScene* scene,
    QWidget* parent)
    : QGraphicsView{scene, parent}, m_app{ctx}, m_opengl{ctx.applicationSettings.opengl}
{
  m_lastwheel = std::chrono::steady_clock::now();

  setAlignment(Qt::AlignTop | Qt::AlignLeft);
  setFrameStyle(0);
  setDragMode(QGraphicsView::NoDrag);
  setAcceptDrops(true);

  setRenderHints(
      QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
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
  if (auto bar = horizontalScrollBar())
  {
    bar->setValue(bar->value() + dx);
    if (m_opengl)
      viewport()->update();
  }
}

void ProcessGraphicsView::resizeEvent(QResizeEvent* ev)
{
  QGraphicsView::resizeEvent(ev);
  sizeChanged(size());

  visibleRectChanged(
      QRectF{this->mapToScene(QPoint{}), this->mapToScene(this->rect().bottomRight())});

  if (m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::scrollContentsBy(int dx, int dy)
{
  QGraphicsView::scrollContentsBy(dx, dy);

  this->scene()->update();
  if (dx != 0)
    scrolled(dx);

  visibleRectChanged(
      QRectF{this->mapToScene(QPoint{}), this->mapToScene(this->rect().bottomRight())});

  if (m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::wheelEvent(QWheelEvent* event)
{
  auto t = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::milliseconds>(t - m_lastwheel).count() < 16)
  {
    return;
  }

  m_lastwheel = t;
  QPoint d = event->angleDelta();
  QPointF delta = {d.x() / 8., d.y() / 8.};
  if (m_hZoom)
  {
    horizontalZoom(delta, mapToScene(event->pos()));
    return;
  }
  else if (m_vZoom)
  {
    verticalZoom(delta, mapToScene(event->pos()));
    return;
  }
  struct MyWheelEvent : public QWheelEvent
  {
    MyWheelEvent(const QWheelEvent& other) : QWheelEvent{other}
    {
      p.ry() /= 4.;
      pixelD.ry() /= 4.;
      angleD /= 4.;
      qt4D /= 4.;
    }
  };
  MyWheelEvent e{*event};
  QGraphicsView::wheelEvent(&e);

  if (m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Control)
    m_hZoom = true;
  else if (event->key() == Qt::Key_Shift)
    m_vZoom = true;

  for (auto& plug : m_app.guiApplicationPlugins())
    plug->on_keyPressEvent(*event);
  event->ignore();

  QGraphicsView::keyPressEvent(event);

  if (m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::keyReleaseEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Control)
    m_hZoom = false;
  else if (event->key() == Qt::Key_Shift)
    m_vZoom = false;

  for (auto& plug : m_app.guiApplicationPlugins())
    plug->on_keyReleaseEvent(*event);
  event->ignore();

  QGraphicsView::keyReleaseEvent(event);

  if (m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::focusOutEvent(QFocusEvent* event)
{
  m_hZoom = false;
  m_vZoom = false;
  focusedOut();
  event->ignore();

  QGraphicsView::focusOutEvent(event);

  if (m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::leaveEvent(QEvent* event)
{
  m_hZoom = false;
  m_vZoom = false;
  focusedOut();
  QGraphicsView::leaveEvent(event);

  if (m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::mousePressEvent(QMouseEvent* event)
{
  if (auto dialog = this->scene()->activePanel())
  {
    const auto other = itemAt(event->pos());
    switch (other->type())
    {
      case Dataflow::PortItem::Type:
      case Dataflow::CableItem::Type:
        break;

      default:
      {
        const auto mapped_pos = other->mapToItem(dialog, QPointF{0, 0});
        if (!dialog->contains(mapped_pos))
        {
          delete dialog;
        }
        break;
      }
    }
  }

  QGraphicsView::mousePressEvent(event);
  if (m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::mouseMoveEvent(QMouseEvent* event)
{
  QGraphicsView::mouseMoveEvent(event);

  if (m_opengl)
    viewport()->update();
}

void ProcessGraphicsView::mouseReleaseEvent(QMouseEvent* event)
{
  QGraphicsView::mouseReleaseEvent(event);
  if (m_opengl)
    viewport()->update();
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
  if (!itemAt(event->pos()))
  {
    dropRequested(event->pos(), event->mimeData());
    event->accept();
  }
  else
  {
    QGraphicsView::dropEvent(event);
  }
}

ScenarioDocumentView::ScenarioDocumentView(const score::DocumentContext& ctx, QObject* parent)
    : score::DocumentDelegateView{parent}
    , m_widget{new QWidget}
    , m_scene{m_widget}
    , m_view{ctx.app, &m_scene, m_widget}
    , m_timeRulerView{&m_timeRulerScene}
    , m_timeRuler{new MusicalRuler{&m_timeRulerView}}
    , m_minimapScene{m_widget}
    , m_minimapView{&m_minimapScene}
    , m_minimap{&m_minimapView}
    , m_bar{&m_baseObject}
{
  con(ctx.document.commandStack(), &score::CommandStack::stackChanged, this, [&] {
    m_view.viewport()->update();
  });

#if defined(SCORE_WEBSOCKETS)
  auto wsview = new WebSocketView(m_scene, 9998, this);
#endif

  m_view.setStatusTip("Main score view. Drop things in here.");
  m_timeRulerView.setStatusTip("The time ruler keeps track of time. Scroll by dragging it.");
  m_minimapView.setStatusTip("A minimap which shows an overview of the topmost score");
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
      largeView, &QAction::triggered, this, [this] { setLargeView(); }, Qt::QueuedConnection);
  con(m_minimap, &Minimap::rescale, largeView, &QAction::trigger);

  // Time Ruler

  {
    auto& settings = ctx.app.settings<Scenario::Settings::Model>();

    auto setupTimeRuler = [=](bool b) {
      delete m_timeRuler;
      if (b)
        m_timeRuler = new MusicalRuler{&m_timeRulerView};
      else
        m_timeRuler = new TimeRuler{&m_timeRulerView};

      connect(m_timeRuler, &TimeRuler::rescale, largeView, &QAction::trigger);
      m_timeRulerScene.addItem(m_timeRuler);
      timeRulerChanged();
    };

    con(settings, &Settings::Model::MeasureBarsChanged, this, [=](bool b) { setupTimeRuler(b); });
    setupTimeRuler(settings.getMeasureBars());
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

  m_view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

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
    auto& es = ctx.app.guiApplicationPlugin<ScenarioApplicationPlugin>().editionSettings();
    es.setTool(Scenario::Tool::Select);
  });

  const bool opengl = ctx.app.applicationSettings.opengl;
  if (opengl)
  {
    // m_minimapView.setViewport(new QOpenGLWidget);
    // m_timeRulerView.setViewport(new QOpenGLWidget);
    QGLFormat fmt;
    fmt.setSamples(4);
    auto vp = new QGLWidget{fmt};
    m_view.setViewport(vp);

    // m_minimapView.setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
    // m_timeRulerView.setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
    m_view.setViewportUpdateMode(QGraphicsView::NoViewportUpdate);

    // m_minimapView.viewport()->setUpdatesEnabled(true);
    // m_timeRulerView.viewport()->setUpdatesEnabled(true);
    m_view.viewport()->setUpdatesEnabled(true);

    const auto tcount = QThread::idealThreadCount();
    if (tcount <= 4)
      m_timer = startTimer(30);
    else if (tcount <= 8)
      m_timer = startTimer(16);
    else
      m_timer = startTimer(8);
  }
  else
  {
    // m_minimapView.setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    m_view.setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    // m_timeRulerView.setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
  }
}

ScenarioDocumentView::~ScenarioDocumentView() { }

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
  return QRectF{m_view.mapToScene(viewRect.topLeft()), m_view.mapToScene(viewRect.bottomRight())};
}

void ScenarioDocumentView::showRulers(bool b)
{
  m_minimapView.setVisible(b);
  m_timeRulerView.setVisible(b);
}

void ScenarioDocumentView::timerEvent(QTimerEvent* event)
{
  // m_minimapView.viewport()->update();
  // m_timeRulerView.viewport()->update();
  m_view.viewport()->update();
}
}

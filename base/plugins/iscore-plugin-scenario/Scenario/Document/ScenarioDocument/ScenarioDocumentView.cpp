#include <Scenario/Application/Menus/TransportActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/TimeRuler/MainTimeRuler/TimeRulerView.hpp>

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QBrush>
#include <QFile>
#include <QFlags>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStyleFactory>
#include <QToolBar>
#include <QWidget>
#include <QTimer>
#include <iscore/model/Skin.hpp>

#include "ScenarioDocumentView.hpp"
#include <iscore/widgets/DoubleSlider.hpp>
#include <iscore/widgets/GraphicsProxyObject.hpp>
#include <iscore/widgets/MarginLess.hpp>

#include <QScrollBar>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <Scenario/Document/ScenarioDocument/SnapshotAction.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateView.hpp>
#include <iscore/widgets/TextLabel.hpp>

#if defined(ISCORE_OPENGL)
#include <QOpenGLWidget>
#endif
#if defined(ISCORE_WEBSOCKETS)
#include "WebSocketView.hpp"
#endif
#include <Process/Style/ScenarioStyle.hpp>

namespace Scenario
{
ScenarioDocumentView::ScenarioDocumentView(
    const iscore::GUIApplicationContext& ctx, QObject* parent)
    : iscore::DocumentDelegateView{parent}
    , m_widget{new QWidget}
    , m_scene{m_widget}
    , m_view{&m_scene, m_widget}
    , m_timeRulersView{&m_scene}
    , m_timeRuler{&m_timeRulersView}
    , m_minimapScene{m_widget}
    , m_minimapView{&m_minimapScene}
    , m_minimap{m_minimapView.viewport()}

{
#if defined(ISCORE_WEBSOCKETS)
  auto wsview = new WebSocketView(m_scene, 9998, this);
#endif
#if defined(ISCORE_OPENGL)
  auto vp1 = new QOpenGLWidget;
  m_view.setViewport(vp1);
  auto vp2 = new QOpenGLWidget;
  m_timeRulersView.setViewport(vp2);
#else
  m_view.setAttribute(Qt::WA_PaintOnScreen, true);
  m_timeRulersView.setAttribute(Qt::WA_PaintOnScreen, true);
#endif
  m_widget->addAction(new SnapshotAction{m_scene, m_widget});

  m_timeRulersView.setFixedHeight(20);
  // Transport
  /// Zoom
  QAction* zoomIn = new QAction(tr("Zoom in"), m_widget);
  m_widget->addAction(zoomIn);
  zoomIn->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  zoomIn->setShortcuts({QKeySequence::ZoomIn, tr("Ctrl+=")});
  connect(zoomIn, &QAction::triggered, this, [&] {
    m_minimap.zoomIn();
  });
  QAction* zoomOut = new QAction(tr("Zoom out"), m_widget);
  m_widget->addAction(zoomOut);
  zoomOut->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  zoomOut->setShortcut(QKeySequence::ZoomOut);
  connect(zoomOut, &QAction::triggered, this, [&] {
    m_minimap.zoomOut();
  });
  QAction* largeView = new QAction{tr("Large view"), m_widget};
  m_widget->addAction(largeView);
  largeView->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  largeView->setShortcut(tr("Ctrl+0"));
  connect(largeView, &QAction::triggered, this, [this] {
      m_minimap.setLargeView();
  }, Qt::QueuedConnection);
  con(m_timeRuler, &AbstractTimeRulerView::rescale,
      largeView, &QAction::trigger);
  con(m_minimap, &Minimap::rescale,
      largeView, &QAction::trigger);

  // view layout
  m_scene.addItem(&m_timeRuler);
  m_scene.addItem(&m_baseObject);

  auto lay = new iscore::MarginLess<QVBoxLayout>;
  m_widget->setLayout(lay);
  m_widget->setContentsMargins(0, 0, 0, 0);

  m_minimapView.setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
  m_minimapView.setSceneRect({0, 0, 4000, 100});
  m_minimapScene.addItem(&m_minimap);

  lay->addWidget(&m_minimapView);
  lay->addWidget(&m_timeRulersView);
  lay->addWidget(&m_view);

  lay->setSpacing(1);

  m_view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto& skin = iscore::Skin::instance();
  con(skin, &iscore::Skin::changed, this, [&]() {
    auto& skin = ScenarioStyle::instance();
    m_timeRulersView.setBackgroundBrush(skin.TimeRulerBackground.getColor());
    m_minimapView.setBackgroundBrush(skin.TimeRulerBackground.getColor());
    m_view.setBackgroundBrush(skin.Background.getColor());
  });

  m_widget->setObjectName("ScenarioViewer");

  // Cursors
  con(this->view(), &ProcessGraphicsView::focusedOut,
          this, [&] {
    auto& es = ctx.guiApplicationPlugin<ScenarioApplicationPlugin>()
                   .editionSettings();
    es.setTool(Scenario::Tool::Select);
  });
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
    m_view.mapToScene(viewRect.topLeft()),
    m_view.mapToScene(viewRect.bottomRight())
  };
}

}

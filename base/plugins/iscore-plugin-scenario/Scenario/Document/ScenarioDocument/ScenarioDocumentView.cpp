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
#include <Process/Tools/ProcessGraphicsView.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioScene.hpp>
#include <iscore/widgets/DoubleSlider.hpp>
#include <iscore/widgets/GraphicsProxyObject.hpp>
#include <iscore/widgets/MarginLess.hpp>

#include <QScrollBar>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <Scenario/Document/ScenarioDocument/SnapshotAction.hpp>
#include <Scenario/Document/TimeRuler/TimeRulerGraphicsView.hpp>
#include <Scenario/Document/Minimap/Minimap.hpp>
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
    , m_scene{new ScenarioScene{m_widget}}
    , m_view{new ProcessGraphicsView{m_scene, m_widget}}
    , m_baseObject{new BaseGraphicsObject}
    , m_timeRulersView{new TimeRulerGraphicsView{m_scene}}
{
#if defined(ISCORE_WEBSOCKETS)
  auto wsview = new WebSocketView(m_scene, 9998, this);
#endif
#if defined(ISCORE_OPENGL)
  auto vp1 = new QOpenGLWidget;
  m_view->setViewport(vp1);
  auto vp2 = new QOpenGLWidget;
  m_timeRulersView->setViewport(vp2);
#else
  m_view->setAttribute(Qt::WA_PaintOnScreen, true);
  m_timeRulersView->setAttribute(Qt::WA_PaintOnScreen, true);
#endif
  m_timeRuler = new TimeRulerView{m_timeRulersView};
  m_widget->addAction(new SnapshotAction{*m_scene, m_widget});

  // Transport
  /// Zoom
  m_zoomSlider = new iscore::DoubleSlider{m_widget};
  m_zoomSlider->setObjectName("ZoomSliderWidget");

  connect(
      m_zoomSlider, &iscore::DoubleSlider::valueChanged, this,
      &ScenarioDocumentView::horizontalZoomChanged);

  QAction* zoomIn = new QAction(tr("Zoom in"), m_widget);
  m_widget->addAction(zoomIn);
  zoomIn->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  zoomIn->setShortcuts({QKeySequence::ZoomIn, tr("Ctrl+=")});
  connect(zoomIn, &QAction::triggered, this, [&]() {
    m_zoomSlider->setValue(m_zoomSlider->value() + 0.04);
    emit horizontalZoomChanged(m_zoomSlider->value());
  });
  QAction* zoomOut = new QAction(tr("Zoom out"), m_widget);
  m_widget->addAction(zoomOut);
  zoomOut->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  zoomOut->setShortcut(QKeySequence::ZoomOut);
  connect(zoomOut, &QAction::triggered, this, [&]() {
    m_zoomSlider->setValue(m_zoomSlider->value() - 0.04);
    emit horizontalZoomChanged(m_zoomSlider->value());
  });
  QAction* largeView = new QAction{tr("Large view"), m_widget};
  m_widget->addAction(largeView);
  largeView->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  largeView->setShortcut(tr("Ctrl+0"));
  connect(largeView, &QAction::triggered, this, &ScenarioDocumentView::setLargeView);
  connect(m_timeRuler, &AbstractTimeRulerView::rescale,
          largeView, &QAction::trigger);

  // view layout
  m_scene->addItem(m_timeRuler);
  m_scene->addItem(m_baseObject);

  auto lay = new iscore::MarginLess<QVBoxLayout>;
  m_widget->setLayout(lay);
  m_widget->setContentsMargins(0, 0, 0, 0);

  auto minimap_scene = new QGraphicsScene;
  auto minimap_view = new TimeRulerGraphicsView{minimap_scene};
  minimap_view->setSceneRect({0, 0, 2000, 100});
  m_minimap = new Minimap{minimap_view->viewport()};
  minimap_scene->addItem(m_minimap);

  lay->addWidget(minimap_view);
  lay->addWidget(m_timeRulersView);
  lay->addWidget(m_view);
  lay->addWidget(m_zoomSlider);

  lay->setSpacing(1);

  m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto& skin = iscore::Skin::instance();
  con(skin, &iscore::Skin::changed, this, [&]() {
    auto& skin = ScenarioStyle::instance();
    m_timeRulersView->setBackgroundBrush(skin.TimeRulerBackground.getColor());
    m_view->setBackgroundBrush(skin.Background.getColor());
  });

  m_widget->setObjectName("ScenarioViewer");

  // Cursors
  con(this->view(), &ProcessGraphicsView::focusedOut,
          this, [&] {
    auto& es = ctx.guiApplicationPlugin<ScenarioApplicationPlugin>()
                   .editionSettings();
    es.setTool(Scenario::Tool::Select);
  });

  con(this->view(), &ProcessGraphicsView::sizeChanged, this,
      &ScenarioDocumentView::on_viewSizeChanged);
}

QWidget* ScenarioDocumentView::getWidget()
{
  return m_widget;
}

qreal ScenarioDocumentView::viewWidth() const
{
  return m_view->width();
}

void ScenarioDocumentView::setLargeView()
{
  m_zoomSlider->setValue(0.05);
  emit horizontalZoomChanged(m_zoomSlider->value());
  QTimer::singleShot(0, [=] {
      if(auto hs = view().horizontalScrollBar())
        hs->setValue(0);
  });
}

void ScenarioDocumentView::on_viewSizeChanged(QSize s)
{
  m_timeRuler->setWidth(s.width());
  m_minimap->setWidth(s.width());
  qDebug() << s.width();
}
}

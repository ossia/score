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
#include <QQuickWidget>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStyleFactory>
#include <QToolBar>
#include <QWidget>
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
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateView.hpp>

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
    const iscore::ApplicationContext& ctx, QObject* parent)
    : iscore::DocumentDelegateView{parent}
    , m_widget{new QWidget}
    , m_scene{new QQuickItem}
    , m_view{new ProcessGraphicsView{m_scene, m_widget}}
    , m_baseObject{new BaseGraphicsObject}
    , m_timeRulersView{new TimeRulerGraphicsView{}}
    , m_timeRuler{new TimeRulerView}
{
#if defined(ISCORE_WEBSOCKETS)
  auto wsview = new WebSocketView(m_scene, 9998, this);
#endif

  m_widget->addAction(new SnapshotAction{*m_scene, m_widget});

  // Transport
  auto transportWidget = new QWidget{m_widget};
  transportWidget->setObjectName("ScenarioTransportWidget");
  auto transportLayout = new iscore::MarginLess<QGridLayout>{transportWidget};

  /// Zoom
  m_zoomSlider = new iscore::DoubleSlider{transportWidget};
  m_zoomSlider->setObjectName("ZoomSliderWidget");

  connect(
      m_zoomSlider, &iscore::DoubleSlider::valueChanged, this,
      &ScenarioDocumentView::horizontalZoomChanged);

  QLabel* zoomLabel = new QLabel{tr("Zoom")};
  zoomLabel->setObjectName("ScenarioZoomLabel");
  transportLayout->addWidget(zoomLabel, 0, 1);
  transportLayout->addWidget(m_zoomSlider, 0, 2);

  QAction* zoomIn = new QAction(tr("Zoom in"), m_widget);
  m_widget->addAction(zoomIn);
  zoomIn->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  zoomIn->setShortcuts({QKeySequence::ZoomIn, tr("Ctrl+=")});
  connect(zoomIn, &QAction::triggered, this, [&]() {
    m_zoomSlider->setValue(m_zoomSlider->value() + 0.05);
    emit horizontalZoomChanged(m_zoomSlider->value());
  });
  QAction* zoomOut = new QAction(tr("Zoom out"), m_widget);
  m_widget->addAction(zoomOut);
  zoomOut->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  zoomOut->setShortcut(QKeySequence::ZoomOut);
  connect(zoomOut, &QAction::triggered, this, [&]() {
    m_zoomSlider->setValue(m_zoomSlider->value() - 0.05);
    emit horizontalZoomChanged(m_zoomSlider->value());
  });
  QAction* largeView = new QAction{tr("Large view"), m_widget};
  m_widget->addAction(largeView);
  largeView->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  largeView->setShortcut(tr("Ctrl+0"));
  connect(largeView, &QAction::triggered, this, [&]() {
    m_zoomSlider->setValue(0.05);
    emit horizontalZoomChanged(m_zoomSlider->value());
  });

  // view layout
  m_timeRuler->setParentItem(m_scene);
  m_baseObject->setParentItem(m_scene);

  auto lay = new QVBoxLayout;
  m_widget->setLayout(lay);

  lay->addWidget(m_timeRulersView);
  lay->addWidget(m_view);
  lay->addWidget(transportWidget);

  lay->setSpacing(1);

  //m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  //m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  connect(
      m_view, &ProcessGraphicsView::scrolled, this,
      &ScenarioDocumentView::horizontalPositionChanged);

  auto& skin = iscore::Skin::instance();
  con(skin, &iscore::Skin::changed, this, [&]() {
    auto& skin = ScenarioStyle::instance();
    m_timeRulersView->setClearColor(skin.TimeRulerBackground.getColor());
    m_view->setClearColor(skin.Background.getColor());
  });

  m_widget->setObjectName("ScenarioViewer");

  // Cursors
  auto& es = ctx.applicationPlugin<ScenarioApplicationPlugin>()
                 .editionSettings();
  con(es, &EditionSettings::toolChanged, this, [=](Scenario::Tool t) {
    switch (t)
    {
      case Scenario::Tool::Select:
        m_view->window()->unsetCursor();
        break;
      case Scenario::Tool::Create:
        m_view->window()->setCursor(QCursor(Qt::CrossCursor));
        break;
      case Scenario::Tool::Play:
        m_view->window()->setCursor(QCursor(Qt::PointingHandCursor));
        break;
      default:
        m_view->window()->unsetCursor();
        break;
    }
  });
}

QWidget* ScenarioDocumentView::getWidget()
{
  return m_widget;
}

void ScenarioDocumentView::update()
{
  m_scene->update();
}

qreal ScenarioDocumentView::viewWidth() const
{
  return m_view->width();
}

void ScenarioDocumentView::newLocalTimeRuler()
{
}
}

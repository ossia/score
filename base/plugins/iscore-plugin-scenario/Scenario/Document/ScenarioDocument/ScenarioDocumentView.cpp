#include <Scenario/Application/Menus/TransportActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/TimeRuler/MainTimeRuler/TimeRulerView.hpp>

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QBrush>
#include <QFlags>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QRect>
#include <QSize>
#include <QString>
#include <QToolBar>
#include <QWidget>
#include <QFile>
#include <QStyleFactory>
#include <Process/Style/Skin.hpp>
#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <Process/Tools/ProcessGraphicsView.hpp>
#include "ScenarioDocumentView.hpp"
#include <Scenario/Document/ScenarioDocument/ScenarioScene.hpp>
#include <iscore/widgets/DoubleSlider.hpp>
#include <iscore/widgets/GraphicsProxyObject.hpp>
#include <iscore/widgets/MarginLess.hpp>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateViewInterface.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <Scenario/Document/ScenarioDocument/SnapshotAction.hpp>
#include <Scenario/Document/TimeRuler/TimeRulerGraphicsView.hpp>
#include <Scenario/Settings/Model.hpp>
class QObject;
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
        const iscore::ApplicationContext& ctx,
        QObject* parent) :
    iscore::DocumentDelegateViewInterface {parent},
    m_widget {new QWidget},
    m_scene {new ScenarioScene{m_widget}},
    m_view {new ProcessGraphicsView{m_scene}},
    m_baseObject {new BaseGraphicsObject},
    m_timeRulersView {new TimeRulerGraphicsView{m_scene}},
    m_timeRuler {new TimeRulerView}
{
#if defined(ISCORE_WEBSOCKETS)
    auto wsview = new WebSocketView(m_scene, 9998, this);
#endif
#if defined(ISCORE_OPENGL)
    QSurfaceFormat f;
    f.setSamples(8);
    auto vp1 = new QOpenGLWidget;
    vp1->setFormat(f);
    m_view->setViewport(vp1);
    auto vp2 = new QOpenGLWidget;
    vp2->setFormat(f);
    m_timeRulersView->setViewport(vp2);
#endif

    m_widget->addAction(new SnapshotAction{*m_scene, m_widget});

    // Transport
    auto transportWidget = new QWidget{m_widget};
    auto transportLayout = new iscore::MarginLess<QGridLayout>{transportWidget};

    QToolBar* transportButtons = new QToolBar;
    // See : http://stackoverflow.com/questions/21363350/remove-gradient-from-qtoolbar-in-os-x
    transportButtons->setStyle(QStyleFactory::create("windows"));

    auto& appPlug = ctx.components.applicationPlugin<ScenarioApplicationPlugin>();
    for(const auto& action : appPlug.pluginActions())
    {
        if(auto trsprt = dynamic_cast<TransportActions*>(action))
        {
            trsprt->populateToolBar(transportButtons);
            for(auto act : trsprt->actions())
            {
                m_view->addAction(act);
            }
            break;
        }
    }

    transportLayout->addWidget(transportButtons, 0, 0);

    /// Zoom
    m_zoomSlider = new DoubleSlider{transportWidget};

    connect(m_zoomSlider, &DoubleSlider::valueChanged,
            this,         &ScenarioDocumentView::horizontalZoomChanged);

    transportLayout->addWidget(new QLabel{tr("Zoom") }, 0, 1);
    transportLayout->addWidget(m_zoomSlider, 0, 2);

    QAction* zoomIn = new QAction(tr("Zoom in"), m_widget);
    m_widget->addAction(zoomIn);
    zoomIn->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    zoomIn->setShortcuts({QKeySequence::ZoomIn, tr("Ctrl+=")});
    connect(zoomIn, &QAction::triggered,
            this, [&] ()
    {
        m_zoomSlider->setValue(m_zoomSlider->value() + 0.05);
        emit horizontalZoomChanged(m_zoomSlider->value());
    });
    QAction* zoomOut = new QAction(tr("Zoom out"), m_widget);
    m_widget->addAction(zoomOut);
    zoomOut->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    zoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOut, &QAction::triggered,
            this, [&] ()
    {
        m_zoomSlider->setValue(m_zoomSlider->value() - 0.05);
        emit horizontalZoomChanged(m_zoomSlider->value());
    });
    QAction* largeView = new QAction{tr("Large view"), m_widget};
    m_widget->addAction(largeView);
    largeView->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    largeView->setShortcut(tr("Ctrl+0"));
    connect(largeView, &QAction::triggered,
            this, [&] ()
    {
        m_zoomSlider->setValue(0.05);
        emit horizontalZoomChanged(m_zoomSlider->value());
    });

    // view layout
    m_scene->addItem(m_timeRuler);
    m_scene->addItem(m_baseObject);

    auto lay = new QVBoxLayout;
    m_widget->setLayout(lay);

    lay->addWidget(m_timeRulersView);
    lay->addWidget(m_view);
    lay->addWidget(transportWidget);

    lay->setSpacing(1);

    connect(m_view, &ProcessGraphicsView::scrolled,
            this,   &ScenarioDocumentView::horizontalPositionChanged);

    auto& skin = Skin::instance();
    con(skin, &Skin::changed,
        this, [&] () {
        auto& skin = ScenarioStyle::instance();
        m_timeRulersView->setBackgroundBrush(skin.TimeRulerBackground.getBrush());
        m_view->setBackgroundBrush(skin.Background.getBrush());
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

void ScenarioDocumentView::newLocalTimeRuler()
{
}
}

#include "BaseElementView.hpp"

#include <QLabel>
#include <QGridLayout>

#include "Widgets/DoubleSlider.hpp"
#include "Widgets/GraphicsProxyObject.hpp"
#include "Document/TimeRuler/MainTimeRuler/TimeRulerView.hpp"
#include "Document/TimeRuler/LocalTimeRuler/LocalTimeRulerView.hpp"

#include "Control/ScenarioControl.hpp"
#include "Control/Menus/TransportActions.hpp"

#include <QStyleFactory>
#if defined(ISCORE_OPENGL)
#include <QGLWidget>
#endif
#if defined(ISCORE_WEBSOCKETS)
#include "WebSocketView.hpp"
#endif
#include <ProcessInterface/Style/ScenarioStyle.hpp>
#include <QSvgGenerator>
#include <QApplication>
#include <QAction>
#include <QMimeData>
#include <QClipboard>
BaseElementView::BaseElementView(QObject* parent) :
    iscore::DocumentDelegateViewInterface {parent},
    m_widget {new QWidget},
    m_scene {new QGraphicsScene{m_widget}},
    m_view {new ScenarioBaseGraphicsView{m_scene}},
    m_baseObject {new GraphicsProxyObject},
    m_timeRuler {new TimeRulerView}
{
#if defined(ISCORE_WEBSOCKETS)
    auto wsview = new WebSocketView(m_scene, 9998, this);
#endif
    m_timeRulersView = new QGraphicsView{m_scene};
#if defined(ISCORE_OPENGL)
    auto vp1 = new QGLWidget;
    vp1->setFormat(QGLFormat(QGL::SampleBuffers));
    m_view->setViewport(vp1);
    auto vp2 = new QGLWidget;
    vp2->setFormat(QGLFormat(QGL::SampleBuffers));
    m_timeRulersView->setViewport(vp2);
#endif

    QAction* snapshot = new QAction("Scenario Screenshot", m_widget);
    m_widget->addAction(snapshot);
    snapshot->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    snapshot->setShortcut(Qt::Key_F10);
    connect(snapshot, &QAction::triggered,
            this, [&] () {
        QBuffer b;
        QSvgGenerator p;
        p.setOutputDevice(&b);
        p.setSize(QSize(1024,768));
        p.setViewBox(QRect(0,0,1024,768));
        QPainter painter;
        painter.begin(&p);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        m_scene->render(&painter);
        painter.end();

        QMimeData * d = new QMimeData;
        d->setData("image/svg+xml",b.buffer());
        QApplication::clipboard()->setMimeData(d,QClipboard::Clipboard);
    });
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    // Configuration

    m_timeRulersView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_timeRulersView->setAttribute(Qt::WA_OpaquePaintEvent);
    m_timeRulersView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_timeRulersView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_timeRulersView->setFocusPolicy(Qt::NoFocus);
    m_timeRulersView->setSceneRect(0, -70, 800, 35);
    m_timeRulersView->setFixedHeight(40);
    m_timeRulersView->setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
    m_timeRulersView->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_timeRulersView->setBackgroundBrush(QBrush(ScenarioStyle::instance().TimeRulerBackground));
    //*/

    // Transport
    auto transportWidget = new QWidget{m_widget};
    auto transportLayout = new QGridLayout;

    QToolBar* transportButtons = new QToolBar;
    // See : http://stackoverflow.com/questions/21363350/remove-gradient-from-qtoolbar-in-os-x
    transportButtons->setStyle(QStyleFactory::create("windows"));

    for(const auto& action : ScenarioControl::instance()->pluginActions())
    {
        if(auto trsprt = dynamic_cast<TransportActions*>(action))
        {
            trsprt->populateToolBar(transportButtons);
            break;
        }
    }

    transportLayout->addWidget(transportButtons, 0, 0);

    /// Zoom
    m_zoomSlider = new DoubleSlider{transportWidget};

    connect(m_zoomSlider, &DoubleSlider::valueChanged,
            this,         &BaseElementView::horizontalZoomChanged);

    transportLayout->addWidget(new QLabel{tr("Zoom") }, 0, 1);
    transportLayout->addWidget(m_zoomSlider, 0, 2);
    transportLayout->setContentsMargins(0, 0, 0, 0);

    transportWidget->setLayout(transportLayout);

    // view layout
    m_scene->addItem(m_timeRuler);
    m_scene->addItem(m_baseObject);

    auto lay = new QVBoxLayout;
    m_widget->setLayout(lay);

    lay->addWidget(m_timeRulersView);
    lay->addWidget(m_view);
    lay->addWidget(transportWidget);

    lay->setSpacing(1);

    connect(m_view, &ScenarioBaseGraphicsView::scrolled,
            this,   &BaseElementView::horizontalPositionChanged);
}

QWidget* BaseElementView::getWidget()
{
    return m_widget;
}

void BaseElementView::update()
{
    m_scene->update();
}

void BaseElementView::newLocalTimeRuler()
{
}

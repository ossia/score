#include "BaseElementView.hpp"

#include <QLabel>
#include <QGridLayout>

#include "Widgets/DoubleSlider.hpp"
#include "Widgets/AddressBar.hpp"
#include "Widgets/GraphicsProxyObject.hpp"
#include "Document/TimeRuler/MainTimeRuler/TimeRulerView.hpp"
#include "Document/TimeRuler/LocalTimeRuler/LocalTimeRulerView.hpp"

BaseElementView::BaseElementView(QObject* parent) :
    iscore::DocumentDelegateViewInterface {parent},
    m_widget {new QWidget},
    m_scene {new QGraphicsScene{m_widget}},
    m_view {new SizeNotifyingGraphicsView{m_scene}},
    m_baseObject {new GraphicsProxyObject},
    m_addressBar {new AddressBar{nullptr}},
    m_timeRuler {new TimeRulerView},
    m_localTimeRuler {new LocalTimeRulerView}
{
    // Configuration
    m_timeRulersView = new QGraphicsView{m_scene};
    m_timeRulersView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_timeRulersView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_timeRulersView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_timeRulersView->setFocusPolicy(Qt::NoFocus);
    m_timeRulersView->setSceneRect(0, -70, 800, 35);
    m_timeRulersView->setFixedHeight(40);

    // Transport
    auto transportWidget = new QWidget{m_widget};
    auto transportLayout = new QGridLayout;

    /// Zoom
    m_zoomSlider = new DoubleSlider{transportWidget};
    m_zoomSlider->setValue(0.03); // 30 seconds by default on an average screen

    connect(m_zoomSlider, &DoubleSlider::valueChanged,
            this,         &BaseElementView::horizontalZoomChanged);

    transportLayout->addWidget(new QLabel{tr("Zoom") }, 0, 1);
    transportLayout->addWidget(m_zoomSlider, 1, 1);
    transportWidget->setLayout(transportLayout);

    // view layout
    m_scene->addItem(m_localTimeRuler);
    m_scene->addItem(m_timeRuler);
    m_scene->addItem(m_baseObject);

    auto lay = new QVBoxLayout;
    m_widget->setLayout(lay);
    lay->addWidget(m_addressBar);
    lay->addWidget(m_timeRulersView);
    lay->addWidget(m_view);
    lay->addWidget(transportWidget);

    m_timeRulersView->setAutoFillBackground(true);
    connect(m_view, &SizeNotifyingGraphicsView::scrolled,
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
    delete m_localTimeRuler;
    m_localTimeRuler = new LocalTimeRulerView{};
    m_scene->addItem(m_localTimeRuler);
}

#include "BaseElementView.hpp"

#include <QLabel>
#include <QGridLayout>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include "Widgets/DoubleSlider.hpp"
#include "Widgets/AddressBar.hpp"
#include "Widgets/GraphicsProxyObject.hpp"
#include "Document/TimeRuler/TimeRulerView.hpp"
#include <QSlider>
#include <QDebug>
#include <QPushButton>

BaseElementView::BaseElementView(QObject* parent) :
    iscore::DocumentDelegateViewInterface {parent},
    m_widget {new QWidget{}},
    m_scene {new QGraphicsScene{this}},
    m_view {new SizeNotifyingGraphicsView{m_scene}},
    m_baseObject {new GraphicsProxyObject{}},
    m_addressBar {new AddressBar{nullptr}},
    m_timeRuler {new TimeRulerView{} },
    m_localTimeRuler {new TimeRulerView{} }
{
    // Configuration
    m_scene->setBackgroundBrush(QBrush{m_widget->palette().dark()});
    m_view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_view->setAlignment(Qt::AlignTop | Qt::AlignLeft);

   // QGraphicsScene* timeRulerScene = new QGraphicsScene{this};
    QGraphicsView* timeRulerView = new QGraphicsView{m_scene};

    //timeRulerScene->setBackgroundBrush(QBrush{m_widget->palette().dark() });
    timeRulerView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    timeRulerView->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    timeRulerView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    timeRulerView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    timeRulerView->setSceneRect(0, -70, 800, 70);
    timeRulerView->setMaximumHeight(70);


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
    lay->addWidget(timeRulerView);
    lay->addWidget(m_view);
    lay->addWidget(transportWidget);
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
    m_localTimeRuler = new TimeRulerView{};
    m_scene->addItem(m_localTimeRuler);
}

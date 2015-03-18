#include "BaseElementView.hpp"

#include <QLabel>
#include <QGridLayout>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include "Widgets/DoubleSlider.hpp"
#include "Widgets/AddressBar.hpp"
#include "Widgets/GraphicsProxyObject.hpp"
#include "Widgets/TimeRuler.hpp"
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
    m_timeRuler {new TimeRuler{} }
{
    // Configuration
    m_scene->setBackgroundBrush(QBrush{m_widget->palette().dark() });
    m_view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_view->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    QGraphicsScene* timeRulerScene = new QGraphicsScene{this};
    QGraphicsView* timeRulerView = new QGraphicsView{timeRulerScene};
    timeRulerScene->setBackgroundBrush(QBrush{m_widget->palette().dark() });
    timeRulerView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    timeRulerView->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    timeRulerView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

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
    timeRulerScene->addItem(m_timeRuler);
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

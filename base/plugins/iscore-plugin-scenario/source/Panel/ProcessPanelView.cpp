#include "ProcessPanelView.hpp"
#include <QHBoxLayout>
#include <QGraphicsScene>
#include <Document/BaseElement/Widgets/SizeNotifyingGraphicsView.hpp>
#include "Document/BaseElement/Widgets/DoubleSlider.hpp"
ProcessPanelView::ProcessPanelView(QObject* parent):
    iscore::PanelView{parent}
{
    m_widget = new QWidget;
    m_scene = new QGraphicsScene(this);

    m_widget->setLayout(new QVBoxLayout);
    m_view = new SizeNotifyingGraphicsView{m_scene};

    m_widget->layout()->addWidget(m_view);

    //m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_view, &SizeNotifyingGraphicsView::sizeChanged,
            this,   &ProcessPanelView::sizeChanged);


    m_zoomSlider = new DoubleSlider{m_widget};
    m_zoomSlider->setValue(0.03); // 30 seconds by default on an average screen

    connect(m_zoomSlider, &DoubleSlider::valueChanged,
            this,         &ProcessPanelView::horizontalZoomChanged);
    m_widget->layout()->addWidget(m_zoomSlider);
    m_view->show();
}

QWidget* ProcessPanelView::getWidget()
{
    return m_widget;
}

Qt::DockWidgetArea ProcessPanelView::defaultDock() const
{
    return Qt::BottomDockWidgetArea;
}

int ProcessPanelView::priority() const
{
    return 10;
}

QString ProcessPanelView::prettyName() const
{
    return tr("Process");
}

QGraphicsScene*ProcessPanelView::scene() const
{return m_scene;}

SizeNotifyingGraphicsView*ProcessPanelView::view() const
{ return m_view; }

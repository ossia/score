#include "ProcessPanelView.hpp"
#include <QHBoxLayout>
#include <QGraphicsScene>
#include <Document/BaseElement/Widgets/SizeNotifyingGraphicsView.hpp>

ProcessPanelView::ProcessPanelView(QObject* parent):
    iscore::PanelViewInterface{parent}
{
    m_widget = new QWidget;
    m_scene = new QGraphicsScene(this);

    m_widget->setLayout(new QHBoxLayout);
    m_view = new SizeNotifyingGraphicsView{m_scene};
    m_widget->layout()->addWidget(m_view);
    m_view->show();

    m_scene->setBackgroundBrush(QBrush{m_widget->palette().dark() });
    m_view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_view->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    connect(m_view, &SizeNotifyingGraphicsView::sizeChanged,
            this,   &ProcessPanelView::sizeChanged);

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

QGraphicsScene*ProcessPanelView::scene() const
{return m_scene;}

SizeNotifyingGraphicsView*ProcessPanelView::view() const
{ return m_view; }

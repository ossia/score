#include "SizeNotifyingGraphicsView.hpp"

SizeNotifyingGraphicsView::SizeNotifyingGraphicsView(QGraphicsScene* parent):
    QGraphicsView{parent}
{
    setCacheMode(QGraphicsView::CacheBackground);
    m_graduations = new SceneGraduations{this};
    scene()->addItem(m_graduations);
    /*
            setViewport(new QOpenGLWidget);
            //setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
            setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
            setAlignment(Qt::AlignTop | Qt::AlignLeft);
            */
    m_bg =  qApp->palette("ScenarioPalette").background();
    m_graduations->setColor(m_bg.color().lighter());
    this->setBackgroundBrush(m_bg);
}

void SizeNotifyingGraphicsView::setGrid(QPainterPath&& newGrid)
{
    m_graduations->setGrid(std::move(newGrid));
}

void SizeNotifyingGraphicsView::resizeEvent(QResizeEvent* ev)
{
    QGraphicsView::resizeEvent(ev);
    emit sizeChanged(size());
}

void SizeNotifyingGraphicsView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    emit scrolled(dx);
}

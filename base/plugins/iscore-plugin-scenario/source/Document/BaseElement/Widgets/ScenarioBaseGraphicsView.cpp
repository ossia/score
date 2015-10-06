#include "ScenarioBaseGraphicsView.hpp"
#include <QDebug>
#include <ProcessInterface/Style/ScenarioStyle.hpp>
ScenarioBaseGraphicsView::ScenarioBaseGraphicsView(QGraphicsScene* parent):
    QGraphicsView{parent}
{
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    setAlignment(Qt::AlignTop | Qt::AlignLeft);
    setCacheMode(QGraphicsView::CacheBackground);

    //m_graduations = new SceneGraduations{this};
    //scene()->addItem(m_graduations);

    this->setAttribute(Qt::WA_OpaquePaintEvent);
    //m_graduations->setColor(m_bg.color().lighter());
    this->setBackgroundBrush(ScenarioStyle::instance().Background);
}

void ScenarioBaseGraphicsView::setGrid(QPainterPath&& newGrid)
{
    //m_graduations->setGrid(std::move(newGrid));
}

void ScenarioBaseGraphicsView::resizeEvent(QResizeEvent* ev)
{
    QGraphicsView::resizeEvent(ev);
    emit sizeChanged(size());
}

void ScenarioBaseGraphicsView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);

    this->scene()->update();
    emit scrolled(dx);
}

void ScenarioBaseGraphicsView::wheelEvent(QWheelEvent *event)
{
    QPoint delta = event->angleDelta() / 8;
    if (m_zoomModifier)
    {
        emit zoom(delta, mapToScene(event->pos()));
        return;
    }

    QGraphicsView::wheelEvent(event);
}


void ScenarioBaseGraphicsView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control)
        m_zoomModifier = true;
    event->ignore();

    QGraphicsView::keyPressEvent(event);
}

void ScenarioBaseGraphicsView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control)
        m_zoomModifier = false;
    event->ignore();

    QGraphicsView::keyReleaseEvent(event);
}

void ScenarioBaseGraphicsView::focusOutEvent(QFocusEvent* event)
{
    m_zoomModifier = false;
    event->ignore();

    QGraphicsView::focusOutEvent(event);
}


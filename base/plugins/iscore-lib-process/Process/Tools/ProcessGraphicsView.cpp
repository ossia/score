#include <Process/Style/ScenarioStyle.hpp>
#include <QEvent>
#include <QFlags>
#include <QGraphicsScene>
#include <qnamespace.h>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <QKeyEvent>

#include "ProcessGraphicsView.hpp"

ProcessGraphicsView::ProcessGraphicsView(QGraphicsScene* parent):
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

void ProcessGraphicsView::setGrid(QPainterPath&& newGrid)
{
    //m_graduations->setGrid(std::move(newGrid));
}

void ProcessGraphicsView::resizeEvent(QResizeEvent* ev)
{
    QGraphicsView::resizeEvent(ev);
    emit sizeChanged(size());
}

void ProcessGraphicsView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);

    this->scene()->update();
    emit scrolled(dx);
}

void ProcessGraphicsView::wheelEvent(QWheelEvent *event)
{
    QPoint delta = event->angleDelta() / 8;
    if (m_zoomModifier)
    {
        emit zoom(delta, mapToScene(event->pos()));
        return;
    }

    QGraphicsView::wheelEvent(event);
}


void ProcessGraphicsView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control)
        m_zoomModifier = true;
    event->ignore();

    QGraphicsView::keyPressEvent(event);
}

void ProcessGraphicsView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control)
        m_zoomModifier = false;
    event->ignore();

    QGraphicsView::keyReleaseEvent(event);
}

void ProcessGraphicsView::focusOutEvent(QFocusEvent* event)
{
    m_zoomModifier = false;
    event->ignore();

    QGraphicsView::focusOutEvent(event);
}


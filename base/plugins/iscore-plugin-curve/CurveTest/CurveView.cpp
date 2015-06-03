#include "CurveView.hpp"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

CurveView::CurveView(QGraphicsItem *parent):
    QGraphicsObject{parent}
{
    this->setFlags(ItemClipsChildrenToShape);
    this->setZValue(parent->zValue() + 1);
}


QRectF CurveView::boundingRect() const
{
    return m_rect;
}

void CurveView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{/*
    painter->setPen(Qt::magenta);
    painter->setBrush(Qt::black);
    painter->drawRect(boundingRect());
    */

    if(m_selectArea != QRectF{})
    {
        painter->setPen(Qt::white);
        //painter->setCompositionMode(QPainter::CompositionMode_Xor);
        //painter->setPen(QPen{QColor{0, 0, 0, 127}, 2, Qt::DashLine, Qt::SquareCap, Qt::BevelJoin});

        painter->drawRect(m_selectArea);
    }
}

void CurveView::setSelectionArea(const QRectF& rect)
{
    m_selectArea = rect;
    update();
}

void CurveView::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
        emit pressed(event->scenePos());
}

void CurveView::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    emit moved(event->scenePos());
}

void CurveView::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    emit released(event->scenePos());
}

void CurveView::setRect(const QRectF& theRect)
{
    prepareGeometryChange();
    m_rect = theRect;
}

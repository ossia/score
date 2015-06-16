#include "StateView.hpp"

#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>
#include <QDebug>
static const qreal radiusFull = 10.;
static const qreal radiusVoid = 3.;

StateView::StateView(QGraphicsObject *parent) : QGraphicsObject(parent)
{
    this->setParentItem(parent);

    this->setZValue(parent->zValue() + 10);
    this->setAcceptDrops(true);
    this->setAcceptHoverEvents(true);
}

int StateView::type() const
{
    return QGraphicsItem::UserType + 4;
}

QRectF StateView::boundingRect() const
{
    return {-radiusFull, -radiusFull, 2*radiusFull, 2*radiusFull};
}

void StateView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPen statePen = QPen(Qt::white);
    statePen.setWidth(2);

    painter->setPen(statePen);
    painter->setBrush(Qt::white);
    qreal radius = m_containtMessage ? radiusFull : radiusVoid;
    painter->drawEllipse({0., 0.}, radius, radius);

    painter->setBrush(Qt::NoBrush);
    painter->setPen(Qt::darkYellow);
//    painter->drawRect(boundingRect());
}

void StateView::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    emit pressed(event->scenePos());
}

void StateView::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    emit moved(event->scenePos());
}

void StateView::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    emit released(event->scenePos());
}


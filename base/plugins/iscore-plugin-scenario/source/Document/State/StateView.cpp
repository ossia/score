#include "StateView.hpp"

#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>

static const qreal radiusFull = 10.;
static const qreal radiusVoid = 3.;

StateView::StateView(QGraphicsObject *parent) : QGraphicsObject(parent)
{
    this->setParentItem(parent);

    this->setZValue(parent->zValue() + 1);
    this->setAcceptDrops(true);
    this->setAcceptHoverEvents(true);
    this->setCursor(Qt::CrossCursor);
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
    painter->drawEllipse({0., 0.}, radiusVoid, radiusVoid);
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


#include <QEvent>
#include <QFlags>
#include <QGraphicsSceneEvent>
#include <qnamespace.h>
#include <QPainter>
#include <QKeyEvent>

#include "CurveView.hpp"

class QStyleOptionGraphicsItem;
class QWidget;

namespace Curve
{
View::View(QGraphicsItem *parent):
    QGraphicsObject{parent}
{
    //this->setCursor(Qt::ArrowCursor);
    this->setFlags(ItemClipsChildrenToShape | ItemIsFocusable);

    this->setZValue(1);
}

View::~View()
{
}

void View::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen(Qt::transparent);
    painter->setBrush(Qt::transparent);
    painter->drawRect(boundingRect());
    if(m_selectArea != QRectF{})
    {
        painter->setPen(Qt::white);
        //painter->setCompositionMode(QPainter::CompositionMode_Xor);
        //painter->setPen(QPen{QColor{0, 0, 0, 127}, 2, Qt::DashLine, Qt::SquareCap, Qt::BevelJoin});

        painter->drawRect(m_selectArea);
    }
}

void View::setSelectionArea(const QRectF& rect)
{
    m_selectArea = rect;
    update();
}

void View::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
        emit pressed(event->scenePos());
    event->accept();
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    emit moved(event->scenePos());
    event->accept();
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    emit released(event->scenePos());
    event->accept();
}

void View::keyPressEvent(QKeyEvent* ev)
{
    emit keyPressed(ev->key());
    ev->accept();
}

void View::keyReleaseEvent(QKeyEvent* ev)
{
    emit keyReleased(ev->key());
    ev->accept();
}

void View::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
    emit contextMenuRequested(ev->screenPos(), ev->scenePos());
}

void View::setRect(const QRectF& theRect)
{
    prepareGeometryChange();
    m_rect = theRect;
}
}

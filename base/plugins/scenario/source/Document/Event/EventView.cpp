#include "EventView.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>

EventView::EventView(QGraphicsObject* parent) :
    QGraphicsObject {parent}
{
    this->setParentItem(parent);

    // TODO hack. How to do it properly ? should events be "over" constraints ? maybe +1.5 ?
    this->setZValue(parent->zValue() + 2);

    this->setFlag(ItemIsSelectable);
    this->setAcceptHoverEvents(true);

    m_color = Qt::black;
}

QRectF EventView::boundingRect() const
{
    return { -5, -5, 10, 10};
}

void EventView::paint(QPainter* painter,
                      const QStyleOptionGraphicsItem* option,
                      QWidget* widget)
{
    QColor pen_color = m_color;

    // Rect
    if(isSelected())
    {
        pen_color = Qt::blue;
    }
    else if (isShadow())
    {
        pen_color = Qt::cyan;
    }

    /*	else if(parentItem()->isSelected())
        {
            pen_color = Qt::cyan;
        }
    */
    //painter->drawRect(boundingRect());

    (m_moving ? pen_color.setAlphaF(0.4) : pen_color.setAlphaF(1.0));

    // Ball
    painter->setBrush(pen_color);
    painter->setPen(pen_color);
    painter->drawEllipse(boundingRect().center(), 5, 5);

//	painter->setPen(QPen(QBrush(QColor(0,0,0)), 1, Qt::SolidLine));

}

void EventView::changeColor(QColor newColor)
{
    m_color = newColor;
    this->update();
}

void EventView::setMoving(bool arg)
{
    m_moving = arg;
    update();
}

void EventView::setShadow(bool arg)
{
    m_shadow = arg;
    update();
}


void EventView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    /*
    QGraphicsItem::mousePressEvent(event);

    m_clickedPoint = event->pos();
    emit eventPressed();
    */
    // Should be selectable only when the state machine is in the selection tool.
    event->ignore();
}

void EventView::mouseReleaseEvent(QGraphicsSceneMouseEvent* m)
{
    /*
    QGraphicsObject::mouseReleaseEvent(m);

    emit eventReleased();

    m_moving = false;
    */
    m->ignore();
}

void EventView::mouseMoveEvent(QGraphicsSceneMouseEvent* m)
{
    /*
    QGraphicsObject::mouseMoveEvent(m);

    auto posInScenario = pos() + m->pos() - m_clickedPoint;

    // TODO effet bizarre : un léger déplacement est autorisé la première fois ... D'où est ce que ça sort ?

    if(m->modifiers() == Qt::ShiftModifier)
    {
        posInScenario.setX(pos().x());
    }

    emit eventMoved(posInScenario);


    m_moving = true;
    */
    m->ignore();
}
/*
void EventView::keyPressEvent(QKeyEvent* e)
{
    if(e->key() == Qt::Key_Control)
    {
        emit ctrlStateChanged(true);
    }
}

void EventView::keyReleaseEvent(QKeyEvent* e)
{
    if(e->key() == Qt::Key_Control)
    {
        emit ctrlStateChanged(false);
    }
}
*/
void EventView::hoverEnterEvent(QGraphicsSceneHoverEvent *h)
{
    QGraphicsObject::hoverEnterEvent(h);
    emit eventHoverEnter();
}

void EventView::hoverLeaveEvent(QGraphicsSceneHoverEvent *h)
{
    QGraphicsObject::hoverLeaveEvent(h);
    emit eventHoverLeave();
}

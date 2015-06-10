#include "TimeNodeView.hpp"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QApplication>
#include "TimeNodePresenter.hpp"
TimeNodeView::TimeNodeView(TimeNodePresenter& presenter,
                           QGraphicsObject* parent) :
    QGraphicsObject {parent},
    m_presenter{presenter}
{
    this->setParentItem(parent);
    this->setZValue(parent->zValue() + 1.5);
    this->setAcceptHoverEvents(true);
    this->setCursor(Qt::CrossCursor);

    m_color = qApp->palette("ScenarioPalette").base().color();
}

void TimeNodeView::paint(QPainter* painter,
                         const QStyleOptionGraphicsItem* option,
                         QWidget* widget)
{
    QColor pen_color = m_color;
    QColor highlight = QColor::fromRgbF(0.188235, 0.54902, 0.776471);

    if(isSelected())
    {
        pen_color = highlight;
    }
    if(! isValid())
    {
        pen_color = Qt::red;
    }

    QPen pen{QBrush(pen_color), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin};
    painter->setPen(pen);

    painter->drawRect(QRectF(QPointF(0, m_top), QPointF(0, m_bottom)));
}

QRectF TimeNodeView::boundingRect() const
{
    return { -3., (qreal) m_top, 6., (qreal)(m_bottom - m_top) };
}

void TimeNodeView::setExtremities(int top, int bottom)
{
    m_top = top;
    m_bottom = bottom;
    this->update();
}

void TimeNodeView::setMoving(bool arg)
{
    m_moving = arg;
    update();
}

void TimeNodeView::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void TimeNodeView::changeColor(QColor newColor)
{
    m_color = newColor;
    this->update();
}


void TimeNodeView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.pressed(event->scenePos());
}

void TimeNodeView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.moved(event->scenePos());
}

void TimeNodeView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    emit m_presenter.released(event->scenePos());
}

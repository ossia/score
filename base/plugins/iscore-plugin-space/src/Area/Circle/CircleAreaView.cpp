#include "CircleAreaView.hpp"
#include <QPainter>
#include <QDebug>
CircleAreaView::CircleAreaView(QGraphicsItem *parent):
    QGraphicsItem{parent}
{

}

QRectF CircleAreaView::boundingRect() const
{
    return m_path.boundingRect();
}

void CircleAreaView::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setPen(Qt::black);
    painter->setBrush(Qt::white);
    painter->drawPath(m_path);
}

void CircleAreaView::update(double x0, double y0, double r)
{
    prepareGeometryChange();
    setPos({x0 - r, y0 - r});
    m_path = QPainterPath();
    m_path.addEllipse({r, r}, r, r);
    QGraphicsItem::update();
}

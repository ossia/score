#include "PointerAreaView.hpp"
#include <QPainter>
#include <QDebug>
PointerAreaView::PointerAreaView(QGraphicsItem *parent):
    QGraphicsItem{parent}
{

}

QRectF PointerAreaView::boundingRect() const
{
    return m_path.boundingRect();
}

void PointerAreaView::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setPen(Qt::black);
    painter->setBrush(Qt::white);
    painter->drawPath(m_path);
}

void PointerAreaView::update(double x0, double y0, double r)
{
    prepareGeometryChange();
    setPos({x0 - r, y0 - r});
    m_path = QPainterPath();
    m_path.addEllipse({r, r}, r, r);
    QGraphicsItem::update();
}

#include "PointerAreaView.hpp"
#include <QPainter>
#include <QDebug>
#define SIDE 4
PointerAreaView::PointerAreaView(QGraphicsItem *parent):
    QGraphicsItem{parent}
{

}

QRectF PointerAreaView::boundingRect() const
{
    return {-SIDE, -SIDE, 2*SIDE, 2*SIDE};
}

void PointerAreaView::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setPen(Qt::black);
    painter->setBrush(Qt::white);
    painter->drawEllipse(boundingRect());
}

void PointerAreaView::update(double x0, double y0)
{
    setPos({x0, y0});
    QGraphicsItem::update();
}

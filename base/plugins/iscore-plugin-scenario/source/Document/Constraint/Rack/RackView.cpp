#include "RackView.hpp"

#include <iscore/tools/NamedObject.hpp>

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

RackView::RackView(QGraphicsObject* parent) :
    QGraphicsObject {parent}
{
    this->setZValue(parent->zValue() + 1);
}

QRectF RackView::boundingRect() const
{
    return {0,
            0,
            qreal(m_width),
            qreal(m_height)
           };
}

void RackView::paint(QPainter* ,
                    const QStyleOptionGraphicsItem* ,
                    QWidget* )
{
}

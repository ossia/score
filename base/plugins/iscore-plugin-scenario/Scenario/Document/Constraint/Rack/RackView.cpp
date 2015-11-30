#include <QtGlobal>

#include "RackView.hpp"

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

RackView::RackView(QGraphicsObject* parent) :
    QGraphicsObject {parent}
{
    this->setZValue(1);
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

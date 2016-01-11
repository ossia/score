#include <QtGlobal>

#include "RackView.hpp"

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;


namespace Scenario
{
RackView::RackView(QGraphicsObject* parent) :
    QGraphicsObject {parent}
{
    this->setZValue(1);
}

QRectF RackView::boundingRect() const
{
    return {0,
            0,
            m_width,
            m_height
           };
}

void RackView::paint(QPainter* ,
                    const QStyleOptionGraphicsItem* ,
                    QWidget* )
{
}
}

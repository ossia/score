#include "BoxView.hpp"

#include <iscore/tools/NamedObject.hpp>

#include <QPainter>
#include <QGraphicsScene>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>

BoxView::BoxView(QGraphicsObject* parent) :
    QGraphicsObject {parent}
{
    this->setZValue(parent->zValue() + 1);
}

QRectF BoxView::boundingRect() const
{
    return {0,
            0,
            qreal(m_width),
            qreal(m_height)
           };
}

void BoxView::paint(QPainter* painter,
                    const QStyleOptionGraphicsItem* option,
                    QWidget* widget)
{
    QRectF header {0, 0, boundingRect().width(), 20};
    QFont f;
    f.setBold(true);

    painter->setFont(f);
    painter->setBrush(Qt::gray);
    painter->setPen(Qt::black);
    painter->drawRect(header);
    painter->drawText(header, Qt::AlignCenter, m_text);
    //painter->drawText(boundingRect(), "Box");
    //painter->drawRect(boundingRect());
}

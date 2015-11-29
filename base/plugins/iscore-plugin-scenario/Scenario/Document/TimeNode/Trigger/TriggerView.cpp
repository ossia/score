#include <qgraphicssvgitem.h>

#include "TriggerView.hpp"

class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

TriggerView::TriggerView(QGraphicsItem *parent):
    QGraphicsObject{parent}
{
    setFlag(ItemStacksBehindParent, true);
    m_item = new QGraphicsSvgItem(":/images/trigger.svg");

    m_item->setScale(1.5);
    m_item->setParentItem(this);

    //m_item->setPos(-7.5, -25.);
    /*
    m_item->setAcceptedMouseButtons(Qt::NoButton);
    m_item->setActive(false);
    m_item->setEnabled(false);
    */
    /*
    setAcceptedMouseButtons(Qt::NoButton);
    setActive(true);
    setEnabled(trye);
    */
}

QRectF TriggerView::boundingRect() const
{
    return {0, 0, 16, 20};
}

void TriggerView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
    QPen debug;
    debug.setColor(Qt::darkRed);
    debug.setCosmetic(true);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(Qt::darkYellow);
    painter->drawRect(boundingRect());
#endif
}

void TriggerView::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
    emit pressed();
}

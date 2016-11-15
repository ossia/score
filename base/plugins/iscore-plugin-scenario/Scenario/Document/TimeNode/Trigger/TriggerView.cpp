#include "TriggerView.hpp"

class QGraphicsSceneMouseEvent;
#include<QPainter>

class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
TriggerView::TriggerView(QGraphicsItem *parent):
    QGraphicsSvgItem{":/images/trigger.svg", parent}
{
    this->setCacheMode(QGraphicsItem::NoCache);
    this->setScale(1.5);
    setFlag(ItemStacksBehindParent, true);
}

void TriggerView::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
    emit pressed();
}
}

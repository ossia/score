#include "TriggerView.hpp"
#include <QGraphicsSvgItem>
TriggerView::TriggerView(QGraphicsItem *parent):
    QGraphicsItem{parent}
{
    setFlag(ItemStacksBehindParent, true);
    auto itm = new QGraphicsSvgItem(":/images/trigger.svg");
    itm->setScale(1.5);
    itm->setParentItem(this);

    itm->setPos(-7.5, -25.);
    itm->setAcceptedMouseButtons(Qt::NoButton);
    itm->setActive(false);
    itm->setEnabled(false);

    setAcceptedMouseButtons(Qt::NoButton);
    setActive(false);
    setEnabled(false);
}

QRectF TriggerView::boundingRect() const
{
    return {};
}

void TriggerView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{

}

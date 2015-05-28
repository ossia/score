#include "TriggerView.hpp"
#include <QGraphicsSvgItem>
TriggerView::TriggerView(QGraphicsItem *parent):
    QGraphicsItem{parent}
{
    auto itm = new QGraphicsSvgItem(":/images/trigger.svg");
    itm->setParentItem(this);

    itm->setPos(-1920/2 + 1, -1080/2 - 10);
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

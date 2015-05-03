#include "AutomationView.hpp"

#include <QPainter>

AutomationView::AutomationView(QGraphicsObject* parent) :
    ProcessView {parent}
{
    setZValue(parent->zValue() + 1);
    setFlags(ItemClipsChildrenToShape);
}

void AutomationView::paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget)
{

}

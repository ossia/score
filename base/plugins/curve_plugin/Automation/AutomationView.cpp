#include "AutomationView.hpp"

#include <QPainter>

AutomationView::AutomationView(QGraphicsObject* parent) :
    ProcessViewInterface {parent}
{
    setZValue(parent->zValue() + 1);
}

void AutomationView::paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget)
{

}

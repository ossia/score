#include "AutomationView.hpp"

#include <QPainter>
#include <QKeyEvent>
#include <QDebug>
AutomationView::AutomationView(QGraphicsObject* parent) :
    LayerView {parent}
{
    setZValue(parent->zValue() + 1);
    setFlags(ItemClipsChildrenToShape);
}

void AutomationView::paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget)
{

}

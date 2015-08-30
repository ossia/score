#include "AutomationView.hpp"

#include <QPainter>
#include <QKeyEvent>
#include <QDebug>
AutomationView::AutomationView(QGraphicsItem* parent) :
    LayerView {parent}
{
    setZValue(parent->zValue() + 1);
    this->setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);
}

void AutomationView::paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget)
{

}

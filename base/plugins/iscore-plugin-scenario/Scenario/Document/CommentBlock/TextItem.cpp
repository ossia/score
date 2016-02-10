#include "TextItem.hpp"

#include <QGraphicsSceneMouseEvent>

namespace Scenario
{

TextItem::TextItem(QString text, QGraphicsObject* parent):
    QGraphicsTextItem{text, parent}
{
    this->setFlag(QGraphicsItem::ItemIsFocusable);
    this->setDefaultTextColor(Qt::white);
}

void TextItem::focusOutEvent(QFocusEvent* event)
{
    emit focusOut();
}

}

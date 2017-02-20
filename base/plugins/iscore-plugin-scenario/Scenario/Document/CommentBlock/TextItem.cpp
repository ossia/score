#include "TextItem.hpp"

#include <QGraphicsSceneMouseEvent>

namespace Scenario
{

TextItem::TextItem(QString text, QQuickPaintedItem* parent)
    : QGraphicsTextItem{text, parent}
{
  this->setFlag(QQuickPaintedItem::ItemIsFocusable);
  this->setDefaultTextColor(Qt::white);
}

void TextItem::focusOutEvent(QFocusEvent* event)
{
  emit focusOut();
}

void SimpleTextItem::paint(
    QPainter* painter)
{
  //    setPen(m_color.getColor()); -> if enabled, there will be undesirable
  //    antialiasing
  setBrush(m_color.getBrush());
  QGraphicsSimpleTextItem::paint(painter, option, widget);
}
}

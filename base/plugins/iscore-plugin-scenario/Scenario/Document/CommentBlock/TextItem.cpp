#include "TextItem.hpp"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
namespace Scenario
{

TextItem::TextItem(QString text, QQuickPaintedItem* parent)
    : GraphicsItem{parent}
{
  //this->setFlag(QQuickPaintedItem::ItemIsFocusable);
  //this->setDefaultTextColor(Qt::white);
}

void TextItem::paint(QPainter* painter)
{
  painter->drawText(boundingRect(), "TODO m_text");
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

  //setBrush(m_color.getBrush());
  painter->drawText(boundingRect(), "TODO m_text");
  //QGraphicsSimpleTextItem::paint(painter, option, widget);
}
}

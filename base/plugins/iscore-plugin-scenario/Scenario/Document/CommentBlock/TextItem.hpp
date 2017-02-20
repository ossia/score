#pragma once
#include <QColor>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsTextItem>
#include <QPen>
#include <iscore/model/ColorReference.hpp>

namespace Scenario
{
// TODO move these two
class TextItem final : public QGraphicsTextItem
{
  Q_OBJECT
public:
  TextItem(QString text, QQuickPaintedItem* parent);

signals:
  void focusOut();

protected:
  void focusOutEvent(QFocusEvent* event) override;
};

class SimpleTextItem final : public QGraphicsSimpleTextItem
{
public:
  using QGraphicsSimpleTextItem::QGraphicsSimpleTextItem;

  void paint(
      QPainter* painter) override;

  void setColor(iscore::ColorRef c)
  {
    m_color = c;
    update();
  }

private:
  iscore::ColorRef m_color;
};
}

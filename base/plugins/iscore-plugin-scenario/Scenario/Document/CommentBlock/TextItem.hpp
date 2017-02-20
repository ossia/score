#pragma once
#include <QColor>
#include <QGraphicsSimpleTextItem>
#include <iscore/tools/GraphicsItem.hpp>
#include <QPen>
#include <iscore/model/ColorReference.hpp>
#include <iscore/tools/Todo.hpp>

namespace Scenario
{
// TODO move these two
class TextItem final : public GraphicsItem
{
  Q_OBJECT
public:
  TextItem(QString text, QQuickPaintedItem* parent);

  void paint(QPainter *painter) override;
signals:
  void focusOut();

protected:
  void focusOutEvent(QFocusEvent* event) override;
};

class SimpleTextItem final : public GraphicsItem
{
public:
  using GraphicsItem::GraphicsItem;

  void paint(
      QPainter* painter) override;

  void setFont(QFont) { ISCORE_TODO; }
  void setText(QString) { ISCORE_TODO; }
  void setColor(iscore::ColorRef c)
  {
    m_color = c;
    update();
  }

private:
  iscore::ColorRef m_color;
};
}

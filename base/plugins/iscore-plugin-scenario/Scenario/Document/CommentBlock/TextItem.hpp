#pragma once
#include <QColor>
#include <QGraphicsTextItem>
#include <QGlyphRun>
#include <QPen>
#include <iscore/model/ColorReference.hpp>

namespace Scenario
{
class TextItem final : public QGraphicsTextItem
{
  Q_OBJECT
public:
  TextItem(QString text, QGraphicsItem* parent);

signals:
  void focusOut();

protected:
  void focusOutEvent(QFocusEvent* event) override;
};

class SimpleTextItem : public QGraphicsItem
{
public:
  SimpleTextItem(QGraphicsItem*);

  QRectF boundingRect() const final override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) final override;

  void setFont(QFont f);
  void setText(QString s);
  void setColor(iscore::ColorRef c);

private:
  void updateImpl();
  QRectF m_rect;
  iscore::ColorRef m_color;
  QFont m_font;
  QString m_string;
  ossia::optional<QGlyphRun> m_line;
};
}

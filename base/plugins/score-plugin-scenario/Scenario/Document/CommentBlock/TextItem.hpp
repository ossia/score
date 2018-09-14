#pragma once
#include <score/model/ColorReference.hpp>

#include <QColor>
#include <QGlyphRun>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QPen>

#include <score_plugin_scenario_export.h>
#include <wobjectdefs.h>
namespace Scenario
{
class TextItem final : public QGraphicsTextItem
{
  W_OBJECT(TextItem)
public:
  TextItem(QString text, QGraphicsItem* parent);

public:
  void focusOut() W_SIGNAL(focusOut);

protected:
  void focusOutEvent(QFocusEvent* event) override;
};

class SCORE_PLUGIN_SCENARIO_EXPORT SimpleTextItem : public QGraphicsItem
{
public:
  SimpleTextItem(QGraphicsItem*);

  QRectF boundingRect() const final override;
  void paint(
      QPainter* painter, const QStyleOptionGraphicsItem* option,
      QWidget* widget) final override;

  void setFont(QFont f);
  void setText(QString s);
  void setText(std::string_view s);
  void setColor(score::ColorRef c);

private:
  void updateImpl();
  QRectF m_rect;
  score::ColorRef m_color;
  QFont m_font;
  QString m_string;
  QImage m_line;
};

class QGraphicsTextButton : public QObject, public Scenario::SimpleTextItem
{
  W_OBJECT(QGraphicsTextButton)
public:
  QGraphicsTextButton(QString text, QGraphicsItem* parent)
      : SimpleTextItem{parent}
  {
    setText(std::move(text));
  }

public:
  void pressed() W_SIGNAL(pressed);

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override
  {
    pressed();
    event->accept();
  }
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
  {
    event->accept();
  }
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
  {
    event->accept();
  }
};
}

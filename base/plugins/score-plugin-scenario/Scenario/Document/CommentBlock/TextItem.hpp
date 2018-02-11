#pragma once
#include <QColor>
#include <QGraphicsTextItem>
#include <QGlyphRun>
#include <QPen>
#include <score/model/ColorReference.hpp>
#include <QGraphicsSceneMouseEvent>
#include <score_plugin_scenario_export.h>
namespace Scenario
{
class TextItem final : public QGraphicsTextItem
{
  Q_OBJECT
public:
  TextItem(QString text, QGraphicsItem* parent);

Q_SIGNALS:
  void focusOut();

protected:
  void focusOutEvent(QFocusEvent* event) override;
};

class SCORE_PLUGIN_SCENARIO_EXPORT SimpleTextItem : public QGraphicsItem
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
  void setColor(score::ColorRef c);

private:
  void updateImpl();
  QRectF m_rect;
  score::ColorRef m_color;
  QFont m_font;
  QString m_string;
  ossia::optional<QGlyphRun> m_line;
};

class QGraphicsTextButton
    : public QObject
    , public Scenario::SimpleTextItem
{
    Q_OBJECT
  public:
    QGraphicsTextButton(QString text, QGraphicsItem* parent)
      : SimpleTextItem{parent}
    {
      setText(std::move(text));
    }

  Q_SIGNALS:
    void pressed();

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

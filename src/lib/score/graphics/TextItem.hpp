#pragma once
#include <score/model/ColorReference.hpp>

#include <QColor>
#include <QGlyphRun>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QPen>

#include <score_lib_base_export.h>
#include <wobjectdefs.h>
namespace score
{
class SCORE_LIB_BASE_EXPORT TextItem final : public QGraphicsTextItem
{
  W_OBJECT(TextItem)
public:
  TextItem(QString text, QGraphicsItem* parent);

public:
  void focusOut() E_SIGNAL(SCORE_LIB_BASE_EXPORT, focusOut);

protected:
  void focusOutEvent(QFocusEvent* event) override;
};

class SCORE_LIB_BASE_EXPORT SimpleTextItem : public QGraphicsItem
{
public:
  SimpleTextItem(const score::ColorRef& col, QGraphicsItem*);

  QRectF boundingRect() const final override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
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

class SCORE_LIB_BASE_EXPORT QGraphicsTextButton : public QObject,
                                                  public score::SimpleTextItem
{
  W_OBJECT(QGraphicsTextButton)
public:
  QGraphicsTextButton(QString text, QGraphicsItem* parent);

public:
  void pressed() E_SIGNAL(SCORE_LIB_BASE_EXPORT, pressed);

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};
}

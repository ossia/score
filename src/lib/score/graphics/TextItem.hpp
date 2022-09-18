#pragma once
#include <score/model/ColorReference.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>

#include <score_lib_base_export.h>

#include <verdigris>
namespace score
{
class SCORE_LIB_BASE_EXPORT TextItem final : public QGraphicsTextItem
{
  W_OBJECT(TextItem)
public:
  TextItem(QString text, QGraphicsItem* parent);

public:
  void focusOut() E_SIGNAL(SCORE_LIB_BASE_EXPORT, focusOut)

protected:
  void focusOutEvent(QFocusEvent* event) override;
};

class SCORE_LIB_BASE_EXPORT SimpleTextItem
    : public QObject
    , public QGraphicsItem
{
public:
  SimpleTextItem(const score::BrushSet& col, QGraphicsItem*);

  QRectF boundingRect() const final override; 
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      final override;

  void setFont(const QFont& f);
  void setText(const QString& s);
  void setText(std::string_view s);
  void setColor(const score::BrushSet& c);
  const QString& text() const noexcept;

private:
  SimpleTextItem() = delete;
  SimpleTextItem(const SimpleTextItem&) = delete;
  SimpleTextItem(SimpleTextItem&&) = delete;
  SimpleTextItem& operator=(const SimpleTextItem&) = delete;
  SimpleTextItem& operator=(SimpleTextItem&&) = delete;
  void updateImpl();

  QRectF m_rect;
  const score::BrushSet* m_color{};
  QFont m_font;
  QString m_string;
  QImage m_line;
};
}



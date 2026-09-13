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
  W_OBJECT(SimpleTextItem)
public:
  SimpleTextItem(const score::BrushSet& col, QGraphicsItem*);

  QRectF boundingRect() const final override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      final override;

  //! @p f must outlive the item: the item keeps a reference to it so that it
  //! can re-render when the skin changes, the same way setColor() keeps a
  //! reference to its BrushSet. In practice this is always a score::Skin
  //! member, which is a singleton.
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
  //! The skin font this item follows, so a font change re-renders instead of
  //! waiting for the item to be recreated. Owned by the skin.
  const QFont* m_font{};

  //! What actually draws: the skin font with font merging restored. Rebuilt
  //! whenever the label or the skin changes, so painting costs no copy.
  QFont m_paintFont;
  QString m_string;
  QImage m_line;
};

class SCORE_LIB_BASE_EXPORT ClickableTextItem final : public score::SimpleTextItem
{
  W_OBJECT(ClickableTextItem)
public:
  ClickableTextItem(const score::BrushSet& brush, QGraphicsItem* parent);

  void clicked() E_SIGNAL(SCORE_LIB_BASE_EXPORT, clicked)
protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
};
}

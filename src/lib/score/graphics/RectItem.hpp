#pragma once
#include <QGraphicsItem>

#include <score_lib_base_export.h>

#include <verdigris>
namespace score
{
class SCORE_LIB_BASE_EXPORT ResizeableItem
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(ResizeableItem)
public:
  explicit ResizeableItem(QGraphicsItem* parent);
  ~ResizeableItem();

  void sizeChanged(QSizeF sz) E_SIGNAL(SCORE_LIB_BASE_EXPORT, sizeChanged, sz)

  enum { Type = UserType + 80000 };
  int type() const override;
};

class SCORE_LIB_BASE_EXPORT RectItem final : public ResizeableItem
{
  W_OBJECT(RectItem)
  Q_INTERFACES(QGraphicsItem)
public:
  using ResizeableItem::ResizeableItem;
  ~RectItem();

  void setRect(const QRectF& r);
  const QRectF& rect() const noexcept { return m_rect; }
  void setHighlight(bool);
  QRectF boundingRect() const final override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) final override;

  enum { Type = UserType + 80001 };
  int type() const override;
public:
  void clicked() E_SIGNAL(SCORE_LIB_BASE_EXPORT, clicked)

private:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) final override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) final override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) final override;

  QRectF m_rect{};
  bool m_highlight{false};
};

class SCORE_LIB_BASE_EXPORT EmptyRectItem : public ResizeableItem
{
  W_OBJECT(EmptyRectItem)
  Q_INTERFACES(QGraphicsItem)
public:
  explicit EmptyRectItem(QGraphicsItem* parent);
  ~EmptyRectItem();

  void setRect(const QRectF& r);
  const QRectF& rect() const noexcept { return m_rect; }
  QRectF boundingRect() const final override;
  void fitChildrenRect();
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  enum { Type = UserType + 80002 };
  int type() const override;
private:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

protected:
  QRectF m_rect{};
};

class SCORE_LIB_BASE_EXPORT BackgroundItem : public QGraphicsItem
{
public:
  explicit BackgroundItem(QGraphicsItem* parent);
  ~BackgroundItem();

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
  void setRect(const QRectF& r);
  const QRectF& rect() const noexcept { return m_rect; }
  QRectF boundingRect() const final override;

  void fitChildrenRect();

  enum { Type = UserType + 80003 };
  int type() const override;
private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) final override;
  QRectF m_rect{};
};

class SCORE_LIB_BASE_EXPORT EmptyItem final : public QGraphicsItem
{
public:
  explicit EmptyItem(QGraphicsItem* parent);
 ~EmptyItem();

  enum { Type = UserType + 80004 };
  int type() const override;
private:
  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
};

}

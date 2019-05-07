#pragma once
#include <QGraphicsItem>

#include <score_lib_base_export.h>
#include <wobjectdefs.h>
namespace score
{
class SCORE_LIB_BASE_EXPORT ResizeableItem
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(ResizeableItem)
public:
  ResizeableItem(QGraphicsItem* parent);
  ~ResizeableItem();

  void sizeChanged(QSizeF sz)
  W_SIGNAL(sizeChanged, sz);
};

class SCORE_LIB_BASE_EXPORT RectItem
    : public ResizeableItem
{
  W_OBJECT(RectItem)
  Q_INTERFACES(QGraphicsItem)
public:
  using ResizeableItem::ResizeableItem;

  void setRect(const QRectF& r);
  void setHighlight(bool);
  QRectF boundingRect() const final override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) final override;

public:
  void clicked() E_SIGNAL(SCORE_LIB_BASE_EXPORT, clicked);

private:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) final override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) final override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) final override;

  QRectF m_rect{};
  bool m_highlight{false};
};

class SCORE_LIB_BASE_EXPORT EmptyRectItem
    : public ResizeableItem
{
  W_OBJECT(EmptyRectItem)
  Q_INTERFACES(QGraphicsItem)
public:
  EmptyRectItem(QGraphicsItem* parent);
  void setRect(const QRectF& r);
  QRectF boundingRect() const final override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) final override;

public:
  void clicked() E_SIGNAL(SCORE_LIB_BASE_EXPORT, clicked);

private:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) final override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) final override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) final override;
  QRectF m_rect{};
};
}

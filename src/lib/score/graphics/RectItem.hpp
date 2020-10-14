#pragma once
#include <QGraphicsItem>

#include <score_lib_base_export.h>

#include <verdigris>
namespace score
{
class SCORE_LIB_BASE_EXPORT ResizeableItem : public QObject, public QGraphicsItem
{
  W_OBJECT(ResizeableItem)
public:
  ResizeableItem(QGraphicsItem* parent);
  ~ResizeableItem();

  void sizeChanged(QSizeF sz) E_SIGNAL(SCORE_LIB_BASE_EXPORT, sizeChanged, sz)
};

class SCORE_LIB_BASE_EXPORT RectItem final : public ResizeableItem
{
  W_OBJECT(RectItem)
  Q_INTERFACES(QGraphicsItem)
public:
  using ResizeableItem::ResizeableItem;

  void setRect(const QRectF& r);
  void setHighlight(bool);
  QRectF boundingRect() const final override;
  void
  paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) final override;

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
  EmptyRectItem(QGraphicsItem* parent);
  void setRect(const QRectF& r);
  QRectF boundingRect() const final override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

//public:
//  void clicked() E_SIGNAL(SCORE_LIB_BASE_EXPORT, clicked)

private:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) final override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) final override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) final override;

protected:
  QRectF m_rect{};
};

class SCORE_LIB_BASE_EXPORT BackgroundItem final : public QGraphicsItem
{
public:
  BackgroundItem(QGraphicsItem* parent);
  ~BackgroundItem();

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
  void setRect(const QRectF& r);
  QRectF boundingRect() const final override;

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

private:
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

}

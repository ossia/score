#pragma once
#include <QGraphicsItem>
#include <score_lib_base_export.h>
namespace score
{

class SCORE_LIB_BASE_EXPORT RectItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
public:
  using QGraphicsItem::QGraphicsItem;
  void setRect(QRectF r);
  void setHighlight(bool);
  QRectF boundingRect() const final override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) final override;

Q_SIGNALS:
  void clicked();

private:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) final override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) final override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) final override;

  QRectF m_rect{};
  bool m_highlight{false};
};

class SCORE_LIB_BASE_EXPORT EmptyRectItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
public:
  EmptyRectItem(QGraphicsItem* parent);
  void setRect(QRectF r);
  QRectF boundingRect() const final override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) final override;

Q_SIGNALS:
  void clicked();

private:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) final override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) final override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) final override;
  QRectF m_rect{};
};

}

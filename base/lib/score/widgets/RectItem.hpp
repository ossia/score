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
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

Q_SIGNALS:
  void clicked();

private:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  QRectF m_rect{};
  bool m_highlight{false};
};

class SCORE_LIB_BASE_EXPORT EmptyRectItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
public:
  EmptyRectItem(QGraphicsItem* parent);
  void setRect(QRectF r);
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

Q_SIGNALS:
  void clicked();

private:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF m_rect{};
};

}

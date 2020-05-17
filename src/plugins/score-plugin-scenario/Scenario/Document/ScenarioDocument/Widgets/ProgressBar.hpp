#pragma once

#include <QGraphicsItem>
#include <QRect>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

class ProgressBar final : public QGraphicsItem
{
public:
  using QGraphicsItem::QGraphicsItem;
  void setHeight(qreal newHeight);

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
  qreal m_height{};
};

#pragma once

#include <QQuickPaintedItem>
#include <QRect>
#include <QtGlobal>

class QPainter;

class QWidget;

class ProgressBar final : public QQuickPaintedItem
{
public:
  using QQuickPaintedItem::QQuickPaintedItem;
  void setHeight(qreal newHeight);

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter) override;

private:
  qreal m_height{};
};

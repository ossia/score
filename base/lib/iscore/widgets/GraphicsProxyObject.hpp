#pragma once
#include <QBrush>
#include <QDebug>
#include <QQuickPaintedItem>
#include <QObject>
#include <QPainter>
class BaseGraphicsObject final : public QQuickPaintedItem
{
public:
  BaseGraphicsObject(QQuickPaintedItem* parent = nullptr) : QQuickPaintedItem{parent}
  {
    this->setFlag(QQuickPaintedItem::ItemHasNoContents, true);
  }

  QRectF boundingRect() const override
  {
    return {};
  }

  void paint(
      QPainter* painter) override
  {
  }

  void setSelectionArea(const QRectF&)
  {
  }
};

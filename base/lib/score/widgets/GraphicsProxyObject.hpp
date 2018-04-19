#pragma once
#include <QBrush>
#include <QDebug>
#include <QGraphicsItem>
#include <QObject>
#include <QPainter>
class BaseGraphicsObject final
    : public QObject
    , public QGraphicsItem
{
public:
  BaseGraphicsObject(QGraphicsItem* parent = nullptr) : QGraphicsItem{parent}
  {
    this->setFlag(QGraphicsItem::ItemHasNoContents, true);
  }

  ~BaseGraphicsObject() override
  {
  }

  QRectF boundingRect() const override
  {
    return {};
  }

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override
  {
  }

  void setSelectionArea(const QRectF&)
  {
  }
};

#pragma once
#include <score/model/Skin.hpp>
#include <QGraphicsItem>
#include <QPainter>
#include <array>

namespace Scenario
{

class LightTimebar : public QGraphicsItem
{
public:
  LightTimebar()
  {
    setFlag(ItemStacksBehindParent);
    setZValue(-9);
  }

  ~LightTimebar()
  {

  }

  QRectF boundingRect() const
  {
    return {0,0,1,10000};
  }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
  {
    painter->setPen(score::Skin::instance().DarkGray.lighter.pen_cosmetic);
    painter->drawLine(0, 0, 0, boundingRect().bottom());
  }
};

class LighterTimebar : public QGraphicsItem
{
public:
  LighterTimebar()
  {
    setFlag(ItemStacksBehindParent);
    setZValue(-10);
  }

  ~LighterTimebar()
  {

  }

  QRectF boundingRect() const
  {
    return {0,0,1,10000};
  }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
  {
    painter->setPen(score::Skin::instance().Background1.lighter180.pen_cosmetic);
    painter->drawLine(0, 0, 0, boundingRect().bottom());
  }
};

}

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
    setZValue(-10);
  }

  QRectF boundingRect() const
  {
    return {0,0,1,1000};
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
    setZValue(-10);
  }

  QRectF boundingRect() const
  {
    return {0,0,1,1000};
  }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
  {
    painter->setPen(score::Skin::instance().Background1.lighter180.pen_cosmetic);
    painter->drawLine(0, 0, 0, boundingRect().bottom());
  }
};

static std::array<LightTimebar, 200> lightBars;
static std::array<LighterTimebar, 600> lighterBars;

}

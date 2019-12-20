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
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(score::Skin::instance().Background1.lighter180.pen_cosmetic);
    painter->drawLine(0, 0, 0, boundingRect().bottom());
  }
};

class LightBars : public QGraphicsItem
{
public:
  std::vector<QLineF> positions;
  LightBars()
  {
    setFlag(ItemStacksBehindParent);
    setZValue(-9);
    positions.resize(1);
  }

  ~LightBars()
  {

  }

  QRectF boundingRect() const
  {
    return {positions.front().x1(), 0,
            positions.back().x1() - positions.front().x1(), 10000};
  }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
  {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(score::Skin::instance().DarkGray.lighter.pen_cosmetic);
    painter->drawLines(positions.data(), positions.size());
  }

  QLineF& operator[](int i)
  {
    if(i >= positions.size())
    {
      positions.resize((i + 1) * 1.2);
    }
    return positions[i];
  }

  void updateShapes() { prepareGeometryChange(); update(); }
};
class LighterBars : public QGraphicsItem
{
public:
  std::vector<QLineF> positions;
  LighterBars()
  {
    setFlag(ItemStacksBehindParent);
    setZValue(-10);
    positions.resize(1);
  }

  ~LighterBars()
  {

  }

  QRectF boundingRect() const
  {
    return {positions.front().x1(), 0,
            positions.back().x1() - positions.front().x1(), 10000};
  }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
  {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(score::Skin::instance().Background1.lighter180.pen_cosmetic);
    painter->drawLines(positions.data(), positions.size());
  }

  QLineF& operator[](int i)
  {
    if(i >= positions.size())
    {
      positions.resize((i + 1) * 1.2);
    }
    return positions[i];
  }

  void updateShapes() { prepareGeometryChange(); update();}
};
}

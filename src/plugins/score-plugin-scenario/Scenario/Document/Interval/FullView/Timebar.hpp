#pragma once
#include <score/model/Skin.hpp>
#include <QGraphicsItem>
#include <QPainter>
#include <array>

namespace Scenario
{
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

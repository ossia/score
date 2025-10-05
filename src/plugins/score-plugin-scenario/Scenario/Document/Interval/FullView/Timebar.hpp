#pragma once
#include <score/model/Skin.hpp>

#include <ossia/detail/ssize.hpp>

#include <QGraphicsItem>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

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
    setFlag(ItemUsesExtendedStyleOption);
    setZValue(-9);
    positions.resize(1);
  }

  ~LightBars() { }

  QRectF boundingRect() const override
  {
    if(positions.empty())
      return {};
    return {
        positions.front().x1(), 0, positions.back().x1() - positions.front().x1(),
        10000};
  }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override
  {
    if(positions.empty())
      return;

    painter->setRenderHint(QPainter::Antialiasing, true);
#if defined(SCORE_DEBUG_REDRAWS)
    {
      QColor c;
      c.setRedF(0.5 * double(rand()) / RAND_MAX);
      c.setGreenF(0.5 * double(rand()) / RAND_MAX);
      c.setBlueF(0.5 * double(rand()) / RAND_MAX);
      c.setAlphaF(1.);
      painter->setPen(QPen(c, 2));
    }
#else
    painter->setPen(score::Skin::instance().DarkGray.main.pen_cosmetic);
#endif

    int begin = 0;
    int end = positions.size();
    for(int i = 0; i < positions.size(); i++)
    {
      auto x = positions[i].x1();
      if(x >= option->exposedRect.left())
      {
        begin = i;
        break;
      }
    }
    for(int i = begin; i < positions.size(); i++)
    {
      auto x = positions[i].x1();
      if(x > option->exposedRect.right())
      {
        end = i;
        break;
      }
      else
      {
        positions[i].setLine(
            x, option->exposedRect.top(), x, option->exposedRect.bottom());
      }
    }

    if(begin < positions.size() && end - begin > 0)
    {
      painter->drawLines(positions.data() + begin, end - begin);
    }
  }

  QLineF& operator[](int i)
  {
    if(i >= std::ssize(positions))
    {
      positions.resize((i + 1) * 1.2);
    }
    return positions[i];
  }

  void updateShapes()
  {
    prepareGeometryChange();
    update();
  }
  int type() const override { return 90077; }
};

class LighterBars : public QGraphicsItem
{
public:
  std::vector<QLineF> positions;
  LighterBars()
  {
    setFlag(ItemStacksBehindParent);
    setFlag(ItemUsesExtendedStyleOption);
    setZValue(-10);
    positions.resize(1);
  }

  ~LighterBars() { }

  QRectF boundingRect() const override
  {
    if(positions.empty())
      return {};
    return {
        positions.front().x1(), 0, positions.back().x1() - positions.front().x1(),
        10000};
  }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override
  {
    painter->setRenderHint(QPainter::Antialiasing, true);

#if defined(SCORE_DEBUG_REDRAWS)
    {
      QColor c;
      c.setRedF(0.5 * double(rand()) / RAND_MAX);
      c.setGreenF(0.5 * double(rand()) / RAND_MAX);
      c.setBlueF(0.5 * double(rand()) / RAND_MAX);
      c.setAlphaF(1.);
      painter->setPen(QPen(c, 2));
    }
#else
    painter->setPen(score::Skin::instance().DarkGray.darker300.pen_cosmetic);
#endif

    int begin = 0;
    int end = positions.size();
    for(int i = 0; i < positions.size(); i++)
    {
      auto x = positions[i].x1();
      if(x >= option->exposedRect.left())
      {
        begin = i;
        break;
      }
    }
    for(int i = begin; i < positions.size(); i++)
    {
      auto x = positions[i].x1();
      if(x > option->exposedRect.right())
      {
        end = i;
        break;
      }
      else
      {
        positions[i].setLine(
            x, option->exposedRect.top(), x, option->exposedRect.bottom());
      }
    }

    if(begin < positions.size() && end - begin > 0)
      painter->drawLines(positions.data() + begin, end - begin);
  }

  QLineF& operator[](int i)
  {
    if(i >= std::ssize(positions))
    {
      positions.resize((i + 1) * 1.2);
    }
    return positions[i];
  }

  void updateShapes()
  {
    prepareGeometryChange();
    update();
  }
  int type() const override { return 90078; }
};

}

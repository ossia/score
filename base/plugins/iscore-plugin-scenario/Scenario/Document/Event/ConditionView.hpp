#pragma once

#include <QQuickPaintedItem>
#include <QPainterPath>
#include <QRect>
#include <iscore/model/ColorReference.hpp>

class QPainter;

class QWidget;

namespace Scenario
{

class ConditionView final : public QQuickPaintedItem
{
public:
  ConditionView(iscore::ColorRef color, QQuickPaintedItem* parent);

  using QQuickPaintedItem::QQuickPaintedItem;
  QRectF boundingRect() const override;
  void paint(
      QPainter* painter) override;
  void changeHeight(qreal newH);

  void setColor(iscore::ColorRef c)
  {
    m_color = c;
    update();
  }

private:
  iscore::ColorRef m_color;
  QPainterPath m_trianglePath;
  QPainterPath m_Cpath;
  qreal m_height{0};
  qreal m_width{40};
  qreal m_CHeight{27};
};
}

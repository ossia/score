#pragma once

#include <QQuickPaintedItem>
#include <QRect>

class QPainter;

class QWidget;

namespace Scenario
{
class RackView final : public QQuickPaintedItem
{
  Q_OBJECT

public:
  RackView(QQuickPaintedItem* parent);
  virtual ~RackView() = default;

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter) override;

  void setHeight(qreal height)
  {
    prepareGeometryChange();
    m_height = height;
  }

  void setWidth(qreal width)
  {
    prepareGeometryChange();
    m_width = width;
  }

private:
  qreal m_height{};
  qreal m_width{};
};
}

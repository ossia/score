#pragma once
#include <Process/LayerView.hpp>

#include <Spline/SplineModel.hpp>

#include <verdigris>
namespace Process
{
struct Context;
}

namespace Spline
{
class CurveItem;
class View final : public Process::LayerView
{
  W_OBJECT(View)
public:
  View(const ProcessModel& m, const Process::Context& ctx, QGraphicsItem* parent);

  void setSpline(ossia::nodes::spline_data d);
  const ossia::nodes::spline_data& spline() const noexcept;

  void setPlayPercentage(float p);

  void recenter();

  void changed() W_SIGNAL(changed);

private:
  void paint_impl(QPainter*) const override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

  void moveControlPoint(QPointF mouse);

  CurveItem* m_impl{};
  QPointF m_pressedPos{};
};
}

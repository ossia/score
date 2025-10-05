#pragma once
#include <Process/LayerView.hpp>

#include <Spline/Model.hpp>

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

  void setSpline(ossia::spline_data d);
  const ossia::spline_data& spline() const noexcept;

  void setPlayPercentage(float p);

  void enable();
  void disable();

  void recenter();

  void changed() W_SIGNAL(changed);

private:
  void paint_impl(QPainter*) const override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

  CurveItem* m_impl{};
  QPointF m_pressedPos{};
};
}

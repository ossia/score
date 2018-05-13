#pragma once
#include <Automation/Color/GradientAutomModel.hpp>
#include <wobjectdefs.h>
#include <Process/LayerView.hpp>
namespace Gradient
{
class View final : public Process::LayerView
{
  W_OBJECT(View)
public:
  View(QGraphicsItem* parent);

  using gradient_colors = boost::container::flat_map<double, QColor>;
  void setGradient(const gradient_colors& c);
  void setDataWidth(double);
  double dataWidth() const
  {
    return m_dataWidth;
  }

public:
  void setColor(double pos, QColor arg_2) W_SIGNAL(setColor, pos, arg_2);
  void movePoint(double old, double cur) W_SIGNAL(movePoint, old, cur);
  void removePoint(double pos) W_SIGNAL(removePoint, pos);

private:
  void paint_impl(QPainter*) const override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

  boost::container::flat_map<double, QColor> m_colors;
  boost::container::flat_map<double, QColor> m_origColors;
  optional<double> m_clicked;
  double m_dataWidth{};
};
}

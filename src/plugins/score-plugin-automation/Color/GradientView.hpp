#pragma once
#include <Process/LayerView.hpp>

#include <Color/GradientModel.hpp>

#include <verdigris>
namespace Gradient
{
class View final : public Process::LayerView
{
  W_OBJECT(View)
public:
  View(QGraphicsItem* parent);

  using gradient_colors = ossia::flat_map<double, QColor>;
  void setGradient(const gradient_colors& c);
  void setDataWidth(double);
  double dataWidth() const { return m_dataWidth; }

public:
  void setColor(double pos, QColor arg_2)
      E_SIGNAL(SCORE_PLUGIN_AUTOMATION_EXPORT, setColor, pos, arg_2);
  void movePoint(double old, double cur)
      E_SIGNAL(SCORE_PLUGIN_AUTOMATION_EXPORT, movePoint, old, cur);
  void removePoint(double pos) E_SIGNAL(SCORE_PLUGIN_AUTOMATION_EXPORT, removePoint, pos);

private:
  void paint_impl(QPainter*) const override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

  ossia::flat_map<double, QColor> m_colors;
  ossia::flat_map<double, QColor> m_origColors;
  std::optional<double> m_clicked;
  double m_dataWidth{};
};
}

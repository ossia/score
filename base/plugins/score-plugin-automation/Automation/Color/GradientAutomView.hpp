#pragma once
#include <Process/LayerView.hpp>
#include <Automation/Color/GradientAutomModel.hpp>
namespace Gradient
{
class View : public Process::LayerView
{
    Q_OBJECT
  public:
    View(QGraphicsItem* parent);

    using gradient_colors = boost::container::flat_map<double, QColor>;
    void setGradient(const gradient_colors& c);
    void setDataWidth(double);
    double dataWidth() const { return m_dataWidth; }

  signals:
    void setColor(double pos, QColor);
    void movePoint(double old, double cur);
    void removePoint(double pos);

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

#pragma once
#include <Process/LayerView.hpp>
#include <Automation/Spline/SplineAutomModel.hpp>
#include <ossia/editor/automation/tinysplinecpp.h>
namespace Spline
{
class View : public Process::LayerView
{
    Q_OBJECT
  public:
    View(QGraphicsItem* parent);

    void setSpline(ossia::spline_data d)
    {
      if(d != m_spline)
        m_spline = std::move(d);
      updateSpline();
      update();
    }

    const ossia::spline_data& spline() const
    { return m_spline; }

  Q_SIGNALS:
    void changed();

  private:
    void paint_impl(QPainter*) const override;
    void updateSpline();

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

    optional<std::size_t> findControlPoint(QPointF point);
    void addPoint(const QPointF point);
    template<typename T>
    QPointF mapToCanvas(const T& point) const
    {
      return QPointF(point.x() * width(),
                     height() - point.y() * height());
    }
    ossia::spline_point mapFromCanvas(const QPointF &point) const;

    ossia::spline_data m_spline;
    tinyspline::BSpline m_spl;
    optional<std::size_t> m_clicked;
};
}

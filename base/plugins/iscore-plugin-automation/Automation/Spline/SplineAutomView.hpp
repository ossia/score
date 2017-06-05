#pragma once
#include <Process/LayerView.hpp>
#include <Automation/Spline/SplineAutomModel.hpp>
#include <tinysplinecpp.h>
namespace Spline
{
class View : public Process::LayerView
{
    Q_OBJECT
  public:
    View(QGraphicsItem* parent);

    void setSpline(spline_data d)
    {
      if(d != m_spline)
        m_spline = std::move(d);
      updateSpline();
      update();
    }

    const spline_data& spline() const
    { return m_spline; }

  signals:
    void changed();
    void pressed();
    void askContextMenu(const QPoint&, const QPointF&);
    void doubleClicked(QPointF);

  private:
    void paint_impl(QPainter*) const override;
    void updateSpline();

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

    optional<std::size_t> findControlPoint(QPointF point);
    void addPoint(const QPointF point);
    QPointF mapToCanvas(const QPointF& point) const;
    QPointF mapFromCanvas(const QPointF &point) const;

    spline_data m_spline;
    tinyspline::BSpline m_spl;
    optional<std::size_t> m_clicked;
    bool m_block;

};
}

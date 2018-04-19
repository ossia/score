#pragma once
#include <Curve/CurveView.hpp>
#include <Process/LayerView.hpp>
#include <QTextLayout>

namespace Interpolation
{
class View final : public Process::LayerView
{
  Q_OBJECT
public:
  explicit View(QGraphicsItem* parent);
  ~View() override;
  void setCurveView(Curve::View* view)
  {
    m_curveView = view;
  }

private:
  QPixmap pixmap() override;
  void paint_impl(QPainter* painter) const override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  Curve::View* m_curveView;
};
}

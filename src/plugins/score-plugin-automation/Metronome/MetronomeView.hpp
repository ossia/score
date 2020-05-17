#pragma once

#include <Curve/CurveView.hpp>
#include <Process/LayerView.hpp>

#include <verdigris>

class QGraphicsItem;
class QPainter;
class QMimeData;

namespace Metronome
{
class LayerView final : public Process::LayerView
{
  W_OBJECT(LayerView)
public:
  explicit LayerView(QGraphicsItem* parent);
  void setCurveView(Curve::View* view) { m_curveView = view; }
  ~LayerView() override;

private:
  QPixmap pixmap() noexcept override;
  void paint_impl(QPainter* painter) const override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;
  Curve::View* m_curveView{};
};
}

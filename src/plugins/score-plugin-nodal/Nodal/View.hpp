#pragma once
#include <Process/LayerView.hpp>

#include <QPointF>
#include <QRectF>

#include <score_plugin_deviceexplorer_export.h>

#include <verdigris>

namespace Nodal
{
class View final : public Process::LayerView
{
  W_OBJECT(View)
public:
  explicit View(QGraphicsItem* parent);
  ~View() override;

  void areaSelectRequested(QRectF area, bool cumulation)
      E_SIGNAL(SCORE_PLUGIN_DEVICEEXPLORER_EXPORT, areaSelectRequested, area, cumulation)

private:
  void paint_impl(QPainter*) const override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  QPointF m_rubberBandOrigin{};
  QRectF m_rubberBandRect{};
  bool m_rubberBanding{false};
};
}

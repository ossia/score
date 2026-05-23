#pragma once
#include <Process/LayerView.hpp>

#include <QPointF>
#include <QRectF>

class QGraphicsRectItem;

#include <score_plugin_nodal_export.h>

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
      E_SIGNAL(SCORE_PLUGIN_NODAL_EXPORT, areaSelectRequested, area, cumulation)

private:
  void paint_impl(QPainter*) const override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  QGraphicsRectItem* m_selectionRect{};
  QPointF m_rubberBandOrigin{};
  QRectF m_rubberBandRect{};
  bool m_rubberBanding{false};
};
}

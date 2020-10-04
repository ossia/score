#pragma once
#include <Process/LayerView.hpp>

namespace Nodal
{
class View final : public Process::LayerView
{
public:
  explicit View(QGraphicsItem* parent);
  ~View() override;

private:
  void paint_impl(QPainter*) const override;

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;
};
}

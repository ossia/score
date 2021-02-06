#pragma once
#include <Process/LayerView.hpp>
#include <State/Message.hpp>

namespace Process
{
class Port;
class ControlInlet;
class PortFactoryList;
}
namespace score
{
struct DocumentContext;
}
namespace ControlSurface
{
class View final : public Process::LayerView
{
  W_OBJECT(View)
public:
  explicit View(QGraphicsItem* parent);
  ~View() override;

  void addressesDropped(State::MessageList lst) W_SIGNAL(addressesDropped, lst);

private:
  void paint_impl(QPainter*) const override;

  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* ev) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* ev) override;

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;
};
}

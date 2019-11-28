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
namespace Patternist
{
class View final : public Process::LayerView
{
  W_OBJECT(View)
public:
  explicit View(
             QGraphicsItem* parent);
  ~View() override;

private:
  void paint_impl(QPainter*) const override;

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent *event) override;
};
}

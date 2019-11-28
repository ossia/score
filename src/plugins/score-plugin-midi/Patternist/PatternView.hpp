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
class ProcessModel;
class View final : public Process::LayerView
{
  W_OBJECT(View)
public:
  explicit View(
             const Patternist::ProcessModel& model,
             QGraphicsItem* parent);
  ~View() override;

  void toggled(int lane, int index) W_SIGNAL(toggled, lane, index);
private:
  void paint_impl(QPainter*) const override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  const Patternist::ProcessModel& m_model;
};
}

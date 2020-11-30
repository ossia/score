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
class QGraphicsNoteChooser;
}
namespace Patternist
{
class ProcessModel;
class View final : public Process::LayerView
{
  W_OBJECT(View)
public:
  explicit View(const Patternist::ProcessModel& model, QGraphicsItem* parent);
  ~View() override;

  void toggled(int lane, int index) W_SIGNAL(toggled, lane, index);
  void noteChanged(int lane, int note) W_SIGNAL(noteChanged, lane, note);
  void noteChangeFinished() W_SIGNAL(noteChangeFinished);

private:
  void updateLanes();
  void paint_impl(QPainter*) const override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  const Patternist::ProcessModel& m_model;

  std::vector<score::QGraphicsNoteChooser*> m_lanes;
};
}

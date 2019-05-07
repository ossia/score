#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <score/graphics/RectItem.hpp>
namespace Dataflow
{
class PortItem;
}

namespace Media::Effect
{
class SCORE_LIB_PROCESS_EXPORT DefaultEffectItem final : public score::RectItem
{
public:
  DefaultEffectItem(
      const Process::ProcessModel& effect,
      const score::DocumentContext& doc,
      QGraphicsItem* root);

  void
  setupInlet(Process::ControlInlet& inlet, const score::DocumentContext& doc);

private:
  void on_controlAdded(const Id<Process::Port>& p);
  void on_controlRemoved(const Process::Port& p);
  void reset();
  const Process::ProcessModel& m_effect;
  const score::DocumentContext& m_ctx;

  std::vector<std::pair<score::EmptyRectItem*, Dataflow::PortItem*>> m_ports;
};
}

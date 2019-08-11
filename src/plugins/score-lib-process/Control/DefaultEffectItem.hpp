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
class SCORE_LIB_PROCESS_EXPORT DefaultEffectItem final : public score::EmptyRectItem
{
public:
  DefaultEffectItem(
      const Process::ProcessModel& effect,
      const score::DocumentContext& doc,
      QGraphicsItem* root);
  ~DefaultEffectItem();

  void setupInlet(Process::ControlInlet& inlet, const score::DocumentContext& doc);
  void setupOutlet(Process::ControlOutlet& inlet, const score::DocumentContext& doc);


private:
  template<typename T>
  void setupPort(T& port, const score::DocumentContext& doc);

  void on_controlAdded(const Id<Process::Port>& p);
  void on_controlRemoved(const Process::Port& p);
  void on_controlOutletAdded(const Id<Process::Port>& p);
  void on_controlOutletRemoved(const Process::Port& p);
  void reset();
  void updateRect();
  double currentColumnX() const;

  const Process::ProcessModel& m_effect;
  const score::DocumentContext& m_ctx;

  struct Port;
  std::vector<Port> m_ports;
};
}

#pragma once
#include <score/model/Identifier.hpp>
#include <score/graphics/RectItem.hpp>
#include <score_lib_process_export.h>

#include <vector>
namespace score
{
class GraphicsLayout;
}
namespace Dataflow
{
class PortItem;
}
namespace Process
{
class ProcessModel;
class PortFactoryList;
struct Context;
class ControlInlet;
class ControlOutlet;
class Port;

class SCORE_LIB_PROCESS_EXPORT DefaultEffectItem final
    : public score::EmptyRectItem
{
public:
  DefaultEffectItem(
      const Process::ProcessModel& effect,
      const Process::Context& doc,
      QGraphicsItem* root);
  ~DefaultEffectItem();

  void setupInlet(
      Process::ControlInlet& inlet,
      const Process::PortFactoryList& portFactory);
  void setupOutlet(
      Process::ControlOutlet& inlet,
      const Process::PortFactoryList& portFactory);

private:
  template <typename T>
  void setupPort(T& port, const Process::PortFactoryList& portFactory);

  void on_controlAdded(const Id<Process::Port>& p);
  void on_controlRemoved(const Process::Port& p);
  void on_controlOutletAdded(const Id<Process::Port>& p);
  void on_controlOutletRemoved(const Process::Port& p);
  void reset();
  void recreate();
  void updateRect();

  score::GraphicsLayout* m_layout{};
  const Process::ProcessModel& m_effect;
  const Process::Context& m_ctx;

  struct Port;
  std::vector<Port> m_ports;
};
}

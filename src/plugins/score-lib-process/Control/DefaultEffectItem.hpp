#pragma once
#include <ossia/detail/small_vector.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/model/Identifier.hpp>

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

class SCORE_LIB_PROCESS_EXPORT DefaultEffectItem final : public score::EmptyRectItem
{
public:
  DefaultEffectItem(
      bool onlyShowUndisplayedPorts, const Process::ProcessModel& effect,
      const Process::Context& doc, QGraphicsItem* root);
  ~DefaultEffectItem();

  void
  setupInlet(Process::ControlInlet& inlet, const Process::PortFactoryList& portFactory);
  void setupOutlet(
      Process::ControlOutlet& inlet, const Process::PortFactoryList& portFactory);

private:
  template <typename T>
  void setupPort(T& port, const Process::PortFactoryList& portFactory);

  void reset();
  void recreate();
  void recreate_full_onlyInlets();
  void recreate_full_onlyOutlets();
  void recreate_full_both(int num_hidden);
  void recreate_fold_onlyInlets();
  void recreate_fold_onlyOutlets();
  void recreate_fold_both();
  void updateRect();
  void relayout();

  score::GraphicsLayout* m_layout{};
  ossia::small_vector<score::GraphicsLayout*, 4> m_allLayouts;
  const Process::ProcessModel& m_effect;
  const Process::Context& m_ctx;

  bool m_onlyUndisplayed{};
  bool m_needRecreate{};
};
}

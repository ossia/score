#pragma once
#include <Process/Dataflow/PortVisibility.hpp>

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
class Inlet;
class Outlet;
class Port;
struct ControlLayout;
struct LayoutBuilderBase;

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
  void recreate_onlyInlets();
  void recreate_onlyOutlets();
  void recreate_both();
  void updateRect();
  void relayout();

  struct PortsToDisplay
  {
    //! In model order; inlets start with `leadingInlets` non-control ports,
    //! which get their own column.
    std::vector<Process::Inlet*> inlets;
    std::vector<Process::Outlet*> outlets;
    Process::ControlPage page;
    int leadingInlets{};
  };

  //! Which ports the current fold state and page call for. Pure: cabling a
  //! control asks this before deciding whether a rebuild is needed at all.
  PortsToDisplay computeDisplayedPorts() const;

  //! Store the result of computeDisplayedPorts() and follow what it depends on.
  void updateDisplayedPorts();

  void onPortWiringChanged();

  //! Port + control, or port + label only when the node is folded.
  template <typename Port_T>
  Process::ControlLayout makeItem(Process::LayoutBuilderBase& b, Port_T& port);

  void createPager();
  void setPage(int page);

  score::GraphicsLayout* m_layout{};
  ossia::small_vector<score::GraphicsLayout*, 4> m_allLayouts;
  const Process::ProcessModel& m_effect;
  const Process::Context& m_ctx;

  //! The ports actually displayed, in model order. Inlets start with
  //! m_leadingInlets non-control ports, which get their own column.
  std::vector<Process::Inlet*> m_shownInlets;
  std::vector<Process::Outlet*> m_shownOutlets;
  int m_leadingInlets{};

  score::EmptyRectItem* m_pager{};
  Process::ControlPage m_controlPage;
  int m_page{};

  bool m_onlyUndisplayed{};
  bool m_needRecreate{};
  bool m_wiringCheckPending{};
};
}

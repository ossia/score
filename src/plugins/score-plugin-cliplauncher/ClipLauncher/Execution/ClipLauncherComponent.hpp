#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <score/model/Identifier.hpp>

#include <ossia/editor/scenario/scenario.hpp>

namespace ossia
{
class scenario;
class time_interval;
class time_sync;
class time_event;
class time_process;
}

namespace Execution
{
class IntervalComponent;
class TimeSyncComponent;
class EventComponent;
class StateComponent;
}

namespace ClipLauncher
{
class CellModel;
class LaneModel;
class ProcessModel;

namespace Execution
{
// Per-cell execution data
struct CellExecData
{
  std::shared_ptr<::Execution::IntervalComponent> intervalComponent;
  std::shared_ptr<::Execution::TimeSyncComponent> startSyncComponent;
  std::shared_ptr<::Execution::TimeSyncComponent> endSyncComponent;
  std::shared_ptr<::Execution::EventComponent> startEventComponent;
  std::shared_ptr<::Execution::EventComponent> endEventComponent;
  std::shared_ptr<::Execution::StateComponent> startStateComponent;
  std::shared_ptr<::Execution::StateComponent> endStateComponent;

  std::shared_ptr<ossia::time_interval> interval;
  std::shared_ptr<ossia::time_sync> startSync;
  std::shared_ptr<ossia::time_sync> endSync;
  std::shared_ptr<ossia::time_event> startEvent;
  std::shared_ptr<ossia::time_event> endEvent;
};

class ClipLauncherComponent final
    : public ::Execution::
          ProcessComponent_T<ClipLauncher::ProcessModel, ossia::scenario>
{
  COMPONENT_METADATA("b2c7d8e9-3f4a-5b6c-8d9e-0a1b2c3d4e5f")
public:
  ClipLauncherComponent(
      ClipLauncher::ProcessModel& element, const ::Execution::Context& ctx,
      QObject* parent);

  ~ClipLauncherComponent() override;

  void cleanup() override;

  std::shared_ptr<ossia::graph_node> gfxForwardNode() const override
  {
    return m_gfxForwardNode;
  }

  // Launch/stop cells
  void launchCell(const Id<CellModel>& cellId, double quantizationRate = 0.);
  void stopCell(const Id<CellModel>& cellId, double quantizationRate = 0.);
  void launchScene(int sceneIndex);

private:
  void setupCell(CellModel& cell);
  void stopAllInLane(int lane, double quantizationRate);

  int laneIndex(const LaneModel& lane) const;

  std::shared_ptr<ossia::scenario> m_scenario;
  std::shared_ptr<ossia::graph_node> m_gfxForwardNode;
  score::hash_map<Id<CellModel>, CellExecData> m_cells;
  score::hash_map<int, Id<CellModel>> m_activeCellPerLane; // lane -> active cell
};

using ClipLauncherComponentFactory
    = ::Execution::ProcessComponentFactory_T<ClipLauncherComponent>;

} // namespace Execution
} // namespace ClipLauncher

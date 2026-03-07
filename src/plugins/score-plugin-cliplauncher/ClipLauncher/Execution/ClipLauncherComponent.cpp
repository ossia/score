#include "ClipLauncherComponent.hpp"

#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>

#include <score/tools/Bind.hpp>

#include <Process/Dataflow/Port.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxForwardNode.hpp>

#include <Scenario/Document/Event/EventExecution.hpp>
#include <Scenario/Document/Interval/ExecutionState.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateExecution.hpp>
#include <Scenario/Document/TimeSync/TimeSyncExecution.hpp>

#include <ClipLauncher/CellModel.hpp>
#include <ClipLauncher/LaneModel.hpp>
#include <ClipLauncher/ProcessModel.hpp>

#include <ossia/dataflow/connection.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/editor/expression/expression.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/nodes/forward_node.hpp>
#include <ossia/editor/scenario/time_sync.hpp>

namespace ClipLauncher::Execution
{

ClipLauncherComponent::ClipLauncherComponent(
    ClipLauncher::ProcessModel& element, const ::Execution::Context& ctx,
    QObject* parent)
    : ProcessComponent_T{element, ctx, "ClipLauncherComponent", parent}
{
  // Create the ossia::scenario that will contain all cell intervals
  m_scenario = std::make_shared<ossia::scenario>();
  m_ossia_process = m_scenario;

  // Create a gfx_forward_node for texture propagation
  if(auto* gfxPlug = ctx.doc.findPlugin<Gfx::DocumentPlugin>())
  {
    m_gfxForwardNode = std::make_shared<Gfx::gfx_forward_node>(gfxPlug->exec);
    m_gfxForwardNode->prepare(*ctx.execState);
    std::weak_ptr<ossia::graph_interface> g_weak = ctx.execGraph;
    in_exec([g_weak, node = m_gfxForwardNode] {
      if(auto graph = g_weak.lock())
        graph->add_node(node);
    });
  }

  // Setup execution structures for each existing cell
  for(auto& cell : element.cells)
  {
    setupCell(cell);
  }

  // Connect lane property change signals for volume
  for(auto& lane : element.lanes)
  {
    int idx = laneIndex(lane);
    con(lane, &LaneModel::volumeChanged, this, [this, idx](double v) {
      // Update gain on all active cells in this lane
      for(auto& [cellId, data] : m_cells)
      {
        auto cellIt = process().cells.find(cellId);
        if(cellIt == process().cells.end())
          continue;
        if(cellIt->lane() == idx && data.interval)
        {
          auto itv = data.interval;
          in_exec([itv, v] {
            auto& ao = static_cast<ossia::nodes::forward_node*>(itv->node.get())->audio_out;
            ao.gain = v;
          });
        }
      }
    });
  }
}

ClipLauncherComponent::~ClipLauncherComponent() { }

void ClipLauncherComponent::cleanup()
{
  // Cleanup all cell components
  for(auto& [id, data] : m_cells)
  {
    if(data.intervalComponent)
      data.intervalComponent->cleanup(data.intervalComponent);
    if(data.startStateComponent)
      data.startStateComponent->cleanup(data.startStateComponent);
    if(data.endStateComponent)
      data.endStateComponent->cleanup(data.endStateComponent);
    if(data.startEventComponent)
      data.startEventComponent->cleanup(data.startEventComponent);
    if(data.endEventComponent)
      data.endEventComponent->cleanup(data.endEventComponent);
    if(data.startSyncComponent)
      data.startSyncComponent->cleanup(data.startSyncComponent);
    if(data.endSyncComponent)
      data.endSyncComponent->cleanup(data.endSyncComponent);
  }

  m_cells.clear();
  m_activeCellPerLane.clear();

  if(m_gfxForwardNode)
  {
    std::weak_ptr<ossia::graph_interface> g_weak = system().execGraph;
    in_exec([g_weak, node = m_gfxForwardNode] {
      if(auto graph = g_weak.lock())
        graph->remove_node(node);
    });
    m_gfxForwardNode.reset();
  }

  m_ossia_process.reset();
  m_scenario.reset();
}

void ClipLauncherComponent::setupCell(CellModel& cell)
{
  CellExecData data;
  auto& scenario = cell.scenarioContainer();

  // Create ossia time_syncs
  data.startSync = std::make_shared<ossia::time_sync>();
  data.endSync = std::make_shared<ossia::time_sync>();

  // Create events on the syncs
  data.startEvent = *data.startSync->emplace(
      data.startSync->get_time_events().begin(), [](auto&&...) {},
      ossia::expressions::make_expression_true());
  data.endEvent = *data.endSync->emplace(
      data.endSync->get_time_events().begin(), [](auto&&...) {},
      ossia::expressions::make_expression_true());

  // Create the time_interval connecting start -> end events
  auto& itv = cell.interval();
  auto& ctx = system();
  auto dur = ctx.time(itv.duration.defaultDuration());
  auto minDur = ctx.time(itv.duration.minDuration());
  auto maxDur = ctx.time(itv.duration.maxDuration());

  data.interval = ossia::time_interval::create(
      ossia::time_interval::exec_callback{},
      *data.startEvent, *data.endEvent, dur, minDur, maxDur);

  // Prepare the interval's node for execution (required before onSetup)
  data.interval->node->prepare(*ctx.execState);

  // Add to scenario
  m_scenario->add_time_sync(data.startSync);
  m_scenario->add_time_sync(data.endSync);
  m_scenario->add_time_interval(data.interval);

  // Ensure all texture outlets in this cell have propagate=true
  // so they automatically route through gfx_forward_node chain
  for(auto& proc : itv.processes)
  {
    for(auto* outlet : proc.outlets())
    {
      if(outlet->type() == Process::PortType::Texture)
      {
        outlet->setPropagate(true);
      }
    }
  }

  // Create execution components for the score model elements
  data.startSyncComponent = std::make_shared<::Execution::TimeSyncComponent>(
      scenario.startTimeSync(), ctx, this);
  data.endSyncComponent = std::make_shared<::Execution::TimeSyncComponent>(
      scenario.endTimeSync(), ctx, this);

  data.startEventComponent = std::make_shared<::Execution::EventComponent>(
      scenario.startEvent(), ctx, this);
  data.endEventComponent = std::make_shared<::Execution::EventComponent>(
      scenario.endEvent(), ctx, this);

  data.startStateComponent = std::make_shared<::Execution::StateComponent>(
      scenario.startState(), data.startEvent, ctx, this);
  data.endStateComponent = std::make_shared<::Execution::StateComponent>(
      scenario.endState(), data.endEvent, ctx, this);

  // Pass m_scenario so child process nodes connect to the audio graph properly
  data.intervalComponent = std::make_shared<::Execution::IntervalComponent>(
      itv, m_scenario, ctx, this);

  // Wire up the components (use makeTrigger() so active/expression/autotrigger are set)
  data.startSyncComponent->onSetup(
      data.startSync, data.startSyncComponent->makeTrigger());
  data.endSyncComponent->onSetup(
      data.endSync, data.endSyncComponent->makeTrigger());

  data.startEventComponent->onSetup(
      data.startEvent, data.startEventComponent->makeExpression(),
      ossia::time_event::offset_behavior::EXPRESSION_TRUE);
  data.endEventComponent->onSetup(
      data.endEvent, data.endEventComponent->makeExpression(),
      ossia::time_event::offset_behavior::EXPRESSION_TRUE);

  data.intervalComponent->onSetup(
      data.intervalComponent, data.interval,
      data.intervalComponent->makeDurations(), false);

  // Connect interval audio output to scenario audio input for audio routing
  {
    auto ossia_itv = data.interval;
    auto proc = m_scenario;

    // Apply per-lane volume via the interval node's gain
    int laneIdx = cell.lane();
    double vol = 1.0;
    {
      int li = 0;
      for(auto& l : process().lanes)
      {
        if(li == laneIdx)
        {
          vol = l.volume();
          break;
        }
        li++;
      }
    }
    auto* fwd = static_cast<ossia::nodes::forward_node*>(ossia_itv->node.get());
    fwd->audio_out.has_gain = true;
    fwd->audio_out.gain = vol;

    in_exec([g = ctx.execGraph, proc, ossia_itv] {
      if(!ossia_itv->node->root_outputs().empty()
         && !proc->node->root_inputs().empty())
      {
        auto cable = g->allocate_edge(
            ossia::immediate_glutton_connection{},
            ossia_itv->node->root_outputs()[0], proc->node->root_inputs()[0],
            ossia_itv->node, proc->node);
        g->connect(cable);
      }
    });
  }

  // Connect interval's gfx_forward_node to clip launcher's gfx_forward_node
  {
    auto itv_gfx_fw = data.intervalComponent->gfxForwardNode();
    auto cl_gfx_fw = m_gfxForwardNode;
    if(itv_gfx_fw && cl_gfx_fw
       && !itv_gfx_fw->root_outputs().empty()
       && !cl_gfx_fw->root_inputs().empty())
    {
      in_exec([g = ctx.execGraph, itv_gfx_fw, cl_gfx_fw] {
        auto cable = g->allocate_edge(
            ossia::immediate_glutton_connection{},
            itv_gfx_fw->root_outputs()[0], cl_gfx_fw->root_inputs()[0],
            itv_gfx_fw, cl_gfx_fw);
        g->connect(cable);
      });
    }
  }

  // Connect interval execution events to cell state
  auto cellId = cell.id();
  con(itv, &Scenario::IntervalModel::executionEvent, this,
      [this, cellId](Scenario::IntervalExecutionEvent ev) {
        auto it = process().cells.find(cellId);
        if(it == process().cells.end())
          return;
        auto& c = *it;
        switch(ev)
        {
          case Scenario::IntervalExecutionEvent::Playing:
            c.setCellState(CellState::Playing);
            break;
          case Scenario::IntervalExecutionEvent::Stopped:
            c.setCellState(CellState::Stopped);
            // Remove from active tracking
            {
              auto laneIt = m_activeCellPerLane.find(c.lane());
              if(laneIt != m_activeCellPerLane.end() && laneIt->second == cellId)
                m_activeCellPerLane.erase(laneIt);
            }
            break;
          default:
            break;
        }
      });

  m_cells.insert({cell.id(), std::move(data)});
}

void ClipLauncherComponent::launchCell(
    const Id<CellModel>& cellId, double quantizationRate)
{
  auto it = m_cells.find(cellId);
  if(it == m_cells.end())
    return;

  auto& cell = process().cells.at(cellId);
  int lane = cell.lane();

  // Stop currently active cell in this lane (exclusive mode by default)
  stopAllInLane(lane, quantizationRate);

  auto proc = m_scenario;
  auto& interval = it->second.interval;

  // Launch the cell on the audio thread
  in_exec([proc, interval, quantizationRate] {
    proc->request_start_interval(*interval, quantizationRate);
  });

  // Track active cell
  m_activeCellPerLane[lane] = cellId;

  // Update cell state
  cell.setCellState(quantizationRate > 0 ? CellState::Queued : CellState::Playing);
}

void ClipLauncherComponent::stopCell(
    const Id<CellModel>& cellId, double quantizationRate)
{
  auto it = m_cells.find(cellId);
  if(it == m_cells.end())
    return;

  auto proc = m_scenario;
  auto& interval = it->second.interval;

  in_exec([proc, interval, quantizationRate] {
    proc->request_stop_interval(*interval, quantizationRate);
  });

  // Remove from active tracking
  auto& element = process();
  auto& cell = element.cells.at(cellId);
  int lane = cell.lane();

  auto laneIt = m_activeCellPerLane.find(lane);
  if(laneIt != m_activeCellPerLane.end() && laneIt->second == cellId)
    m_activeCellPerLane.erase(laneIt);

  cell.setCellState(CellState::Stopping);
}

void ClipLauncherComponent::launchScene(int sceneIndex)
{
  auto& element = process();

  // Use global quantization rate (1.0 = bar, 4.0 = beat, 0.0 = immediate)
  double rate = element.globalQuantization();

  // Stop all currently active cells (will be replaced by new scene)
  for(auto it = m_activeCellPerLane.begin(); it != m_activeCellPerLane.end();)
  {
    auto cellIt = m_cells.find(it->second);
    if(cellIt != m_cells.end())
    {
      auto proc = m_scenario;
      auto& interval = cellIt->second.interval;
      in_exec([proc, interval, rate] {
        proc->request_stop_interval(*interval, rate);
      });
      auto& c = element.cells.at(it->second);
      c.setCellState(CellState::Stopping);
    }
    it = m_activeCellPerLane.erase(it);
  }

  // Launch all cells in the new scene
  for(auto& cell : element.cells)
  {
    if(cell.scene() == sceneIndex)
    {
      launchCell(cell.id(), rate);
    }
  }
}

void ClipLauncherComponent::stopAllInLane(int lane, double quantizationRate)
{
  auto it = m_activeCellPerLane.find(lane);
  if(it != m_activeCellPerLane.end())
  {
    stopCell(it->second, quantizationRate);
  }
}

int ClipLauncherComponent::laneIndex(const LaneModel& lane) const
{
  int idx = 0;
  for(auto& l : process().lanes)
  {
    if(&l == &lane)
      return idx;
    idx++;
  }
  return -1;
}

} // namespace ClipLauncher::Execution

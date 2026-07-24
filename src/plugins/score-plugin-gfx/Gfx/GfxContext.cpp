#include <Gfx/GfxContext.hpp>
#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/Timers.hpp>

#include <ossia/detail/flicks.hpp>

#include <algorithm>
#include <ossia/detail/logger.hpp>
#include <ossia/detail/thread.hpp>

#include <QGuiApplication>
#include <QTimer>
#include <wobjectimpl.h>
namespace Gfx
{
GfxContext::GfxContext(const score::DocumentContext& ctx)
    : m_context{ctx}
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  new_edges.reserve(100);
  edges.reserve(100);

  auto& settings = m_context.app.settings<Gfx::Settings::Model>();
  con(settings, &Gfx::Settings::Model::GraphicsApiChanged, this,
      &GfxContext::recompute_graph);
  con(settings, &Gfx::Settings::Model::SamplesChanged, this,
      &GfxContext::recompute_graph);
  con(settings, &Gfx::Settings::Model::RateChanged, this, &GfxContext::recompute_graph);
  con(settings, &Gfx::Settings::Model::VSyncChanged, this, &GfxContext::recompute_graph);
  con(settings, &Gfx::Settings::Model::BuffersChanged, this,
      &GfxContext::recompute_graph);

  m_graph = new score::gfx::Graph;
  // Hand the session-wide AssetTable down to the Graph so every
  // RenderList it creates can participate in content-hash decode
  // dedup: one decode per asset per session, N uploads.
  m_graph->setAssetTable(&m_assets);

  double rate = m_context.app.settings<Gfx::Settings::Model>().getRate();
  rate = qBound(1.0, rate, 1000.);


  {
    m_no_vsync_timer = m_timers.acquireTimer(this, rate);
    connect(m_no_vsync_timer, &score::HighResolutionTimer::timeout, this, &GfxContext::on_no_vsync_timer, Qt::UniqueConnection);
  }

  // A safety timer necessary to handle graph updates in case we had vsync and lost it
  {
    m_watchdog_timer = m_timers.acquireTimer(this, 20.);
    connect(m_watchdog_timer, &score::HighResolutionTimer::timeout, this, &GfxContext::on_watchdog_timer, Qt::UniqueConnection);
  }
}

GfxContext::~GfxContext()
{
#if defined(SCORE_THREADED_GFX)
  if(m_thread.isRunning())
    m_thread.exit(0);
  m_thread.wait();
#endif

  // Stop all timers before destroying the graph and nodes,
  // to prevent timer callbacks from accessing stale pointers.
  // Render clocks first: their dtors release the shared timers back to the
  // still-live m_timers pool before it is destroyed below.
  m_renderClocks.clear();
  m_vsyncClock.reset();
  m_no_vsync_timer = nullptr;
  m_watchdog_timer = nullptr;
  std::destroy_at(&m_timers);
  std::construct_at(&m_timers);

  delete m_graph;
}

int32_t GfxContext::register_node(std::unique_ptr<score::gfx::Node> node)
{
  // OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  auto next = index++;
  if(next == score::gfx::invalid_node_index)
    next = index++;

  node->nodeId = next;
  tick_commands.enqueue(NodeCommand{NodeCommand::ADD_NODE, next, std::move(node)});

  return next;
}

int32_t GfxContext::register_preview_node(std::unique_ptr<score::gfx::Node> node)
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  auto next = index++;
  if(next == score::gfx::invalid_node_index)
    next = index++;

  node->nodeId = next;
  tick_commands.enqueue(
      NodeCommand{NodeCommand::ADD_PREVIEW_NODE, next, std::move(node)});

  return next;
}

void GfxContext::unregister_node(int32_t idx)
{
  // FIXME! we need to move back to spsc queue for ensured ordering,
  // thus enforcing a thread for node commands
  // OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  if(idx != score::gfx::invalid_node_index)
    tick_commands.enqueue(NodeCommand{NodeCommand::REMOVE_NODE, idx, {}});
}

void GfxContext::destroyOutput(score::gfx::OutputNode* node)
{
  // Synchronous counterpart to the async REMOVE_NODE path: releases the
  // output's RenderList (while its QRhi is still alive), calls destroyOutput()
  // and drops the node from Graph::m_outputs. Safe to call at shutdown, when
  // rendering is stopped and the tick queue will never be drained again.
  if(m_graph && node)
  {
    // Unregister from the render clocks FIRST. A clock keeps raw OutputNode*
    // and its timer tick iterates them calling render(); leaving a node that
    // is about to be freed in that list is a use-after-free as soon as the
    // next queued tick fires — which it does, because tearing an output down
    // (closing a preview, removing a device) pumps the event loop. The async
    // REMOVE_NODE path already does this; this synchronous one must too.
    for(auto it = m_renderClocks.begin(); it != m_renderClocks.end();)
    {
      (*it)->removeOutput(node);
      if((*it)->empty())
        it = m_renderClocks.erase(it);
      else
        ++it;
    }

    m_graph->destroyOutputRenderList(*node);

    // Drop the edges too. Graph::removeNode is, by its own comment, "a pure
    // pointer erase": it leaves m_edges holding Edges that point at this
    // output's Ports. The device is about to free the node and its Ports, so
    // ~Graph -> clearEdges() would then delete those Edges and, in ~Edge,
    // unlink them from the freed Ports. This is the same call the async
    // REMOVE_NODE path makes; it leaves m_nodes alone, which removeNode below
    // handles.
    m_graph->removeNodeAndEdges(node);

    // Also drop it from m_nodes: ~Graph's belt-and-braces loop does
    // dynamic_cast<OutputNode*>(n) over m_nodes, which would deref this freed
    // node's vtable once the device destroys it. removeNode is a pure pointer
    // erase, so it is safe with the (still-alive) node here.
    m_graph->removeNode(node);

    // A device-owned output (offscreen BackgroundNode) is double-owned: the
    // device keeps it in a unique_ptr AND register_node() stored another
    // unique_ptr in `nodes`. The device is about to free it, so RELEASE (do
    // not delete) our copy of the ownership and drop the map entry — otherwise
    // ~GfxContext's `nodes` map frees it a second time (double-free).
    if(auto it = nodes.find(node->nodeId); it != nodes.end())
    {
      (void)it->second.release();
      nodes.erase(it);
    }
  }
}

void GfxContext::unregister_preview_node(int32_t idx)
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  if(idx != score::gfx::invalid_node_index)
    tick_commands.enqueue(NodeCommand{NodeCommand::REMOVE_PREVIEW_NODE, idx, {}});
}

void GfxContext::connect_preview_node(EdgeSpec e)
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  tick_commands.enqueue(EdgeCommand{EdgeCommand::CONNECT_PREVIEW_NODE, e});
}

void GfxContext::disconnect_preview_node(EdgeSpec e)
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  tick_commands.enqueue(EdgeCommand{EdgeCommand::DISCONNECT_PREVIEW_NODE, e});
}

void GfxContext::add_edge(EdgeSpec edge)
{
  auto source_node_it = this->nodes.find(edge.first.node);
  if(source_node_it == this->nodes.end())
    return;
  auto sink_node_it = this->nodes.find(edge.second.node);
  if(sink_node_it == this->nodes.end())
    return;
  if(!source_node_it->second || !sink_node_it->second)
    return;

  auto& source_ports = source_node_it->second->output;
  auto& sink_ports = sink_node_it->second->input;

  // Silently drop malformed edges. A live-coded or half-wired patch can
  // produce an edge whose declared port index doesn't exist on either side
  // (e.g. a shader that parses to zero input ports but the script still
  // issued a `connect(..., 0, consumer, 0)`). Aborting the whole renderer
  // on a script-level wiring mistake is not an option — drop the edge and
  // keep rendering.
  if(edge.first.port >= source_ports.size()
     || edge.second.port >= sink_ports.size())
    return;

  m_graph->addEdge(source_ports[edge.first.port], sink_ports[edge.second.port],
                   edge.type);
}

void GfxContext::remove_edge(EdgeSpec edge)
{
  auto source_node_it = this->nodes.find(edge.first.node);
  if(source_node_it == this->nodes.end())
    return;
  auto sink_node_it = this->nodes.find(edge.second.node);
  if(sink_node_it == this->nodes.end())
    return;
  if(!source_node_it->second || !sink_node_it->second)
    return;

  auto& source_ports = source_node_it->second->output;
  auto& sink_ports = sink_node_it->second->input;
  if(edge.first.port >= source_ports.size()
     || edge.second.port >= sink_ports.size())
    return;

  m_graph->removeEdge(source_ports[edge.first.port],
                      sink_ports[edge.second.port]);
}

void GfxContext::recompute_edges()
{
  m_graph->clearEdges();

  // Snapshot under lock: writer in updateGraph reassigns `edges` under
  // edges_lock on the render-driving thread, while this can be invoked from
  // settings-change signals on the UI thread. Iterating the live container
  // would race with that reassignment.
  ossia::flat_set<EdgeSpec> edges_snapshot;
  ossia::flat_set<EdgeSpec> preview_snapshot;
  {
    std::lock_guard l{edges_lock};
    edges_snapshot = edges;
    preview_snapshot = preview_edges;
  }

  for(auto edge : edges_snapshot)
  {
    add_edge(edge);
  }
  for(auto edge : preview_snapshot)
  {
    add_edge(edge);
  }
}

void GfxContext::recomputeTimers()
{
  // Tear down the render clocks BEFORE nuking the timer pool, so their dtors
  // release the shared timers back to a still-live m_timers.
  m_renderClocks.clear();
  m_vsyncClock.reset();

  // Clear previous timers
  std::destroy_at(&m_timers);
  std::construct_at(&m_timers);
  {
    m_watchdog_timer = m_timers.acquireTimer(this, 20.);
    connect(m_watchdog_timer, &score::HighResolutionTimer::timeout, this, &GfxContext::on_watchdog_timer, Qt::UniqueConnection);
  }
  m_no_vsync_timer = nullptr;

  for(auto& output : m_graph->outputs())
  {
    output->setVSyncCallback({});
  }

  auto& settings = m_context.app.settings<Gfx::Settings::Model>();
  const double settings_rate = settings.getRate();
  const auto api = settings.graphicsApiEnum();

  // Recreate new timers
  // The vsync render loop drives itself through QWindow::requestUpdate(). On
  // Wayland requestUpdate() is frame-callback gated and does not self-heal: a
  // render() cycle that early-returns without committing a buffer breaks the
  // frame-callback chain and the loop stalls for good, leaving the window frozen
  // on its startup black clear. The render itself is fine there -- an offscreen
  // readback of the same frame returns correct pixels -- so the symptom is a
  // black on-screen window despite correct rendering. The timer-driven path
  // below re-drives render() unconditionally (commit-independent) and presents
  // correctly; the compositor supplies pacing on Wayland regardless. xcb, eglfs
  // and the embedded backends keep the swap-chain vsync loop unchanged.
  const bool waylandThrottled
      = QGuiApplication::platformName().contains(QLatin1String("wayland"));
  const bool vsync
      = settings.getVSync() && m_graph->canDoVSync() && !waylandThrottled;

  // Update and render
  // This starts the timer for updating the graph, that is, reading the new parameters.
  if(vsync)
  {
    // Only one thread / renderer, best case, we use it for updating the graph, reading the commands etc.
#if defined(SCORE_THREADED_GFX)
    if(api == Vulkan)
    {
      //:startTimer(rate, Qt::PreciseTimer);
      moveToThread(&m_thread);
      m_thread.start();
    }
#endif
    SCORE_ASSERT(m_graph->outputs().size() == 1);
    SCORE_ASSERT(m_graph->outputs()[0]);
    // Clock #1: the display swap-chain vsync callback (push). Wrapping it in a
    // DisplayVSyncClock is behaviour-identical to calling setVSyncCallback here.
    m_vsyncClock
        = std::make_unique<score::gfx::DisplayVSyncClock>(*m_graph->outputs().front());
    m_vsyncClock->start([this] { updateGraph(); });
  }
  else
  {
    // Multiple renderers, so we have one timer at the highest fps for updating the graph before anything gets rendered.

    // rate in fps
    double rate = settings_rate;

    for(auto& output : m_graph->outputs())
    {
      auto conf = output->configuration();
      if(conf.manualRenderingRate)
      {
        rate = std::max(1000. / *conf.manualRenderingRate, rate);
      }
    }

    rate = qBound(1.0, rate, 1000.);

    m_no_vsync_timer = m_timers.acquireTimer(this, rate);
    connect(m_no_vsync_timer, &score::HighResolutionTimer::timeout, this, &GfxContext::on_no_vsync_timer, Qt::ConnectionType(Qt::UniqueConnection|Qt::QueuedConnection));


    // Clock #2 (the default): the shared wall-timer at manualRenderingRate.
    // Outputs at the same rate coalesce onto one TimerClock / one shared timer,
    // exactly as the old timer->set<OutputNode*> map did. The per-output
    // fan-out closure is the old on_manual_timer body.
    for(auto& output : m_graph->outputs())
    {
      auto conf = output->configuration();
      if(conf.manualRenderingRate)
      {
        const double freq = 1000. / *conf.manualRenderingRate;

        score::gfx::TimerClock* clock{};
        for(auto& c : m_renderClocks)
        {
          if(c->frequency() == freq)
          {
            clock = c.get();
            break;
          }
        }

        if(!clock)
        {
          auto owned = std::make_unique<score::gfx::TimerClock>(m_timers, this, freq);
          clock = owned.get();
          m_renderClocks.push_back(std::move(owned));
          clock->start([clock] {
            for(auto* out : clock->outputs())
              if(out && out->canRender())
                out->render();
          });
        }

        clock->addOutput(output);
      }
    }
  }
}

void GfxContext::recomputeGraphTopology()
{
  recompute_edges();

  auto& settings = m_context.app.settings<Gfx::Settings::Model>();
  const auto api = settings.graphicsApiEnum();

  m_graph->createAllRenderLists(api);
}

void GfxContext::recompute_graph()
{
  // Topology first: refreshes m_graph->outputs() which recomputeTimers reads.
  // Must run before timers because recomputeTimers iterates outputs().
  recomputeGraphTopology();
  recomputeTimers();
}

void GfxContext::add_preview_output(score::gfx::OutputNode& node)
{
  auto& settings = m_context.app.settings<Gfx::Settings::Model>();
  auto api = settings.graphicsApiEnum();

  m_graph->createSingleRenderList(node, api);

  // rate in fps
  double rate = m_context.app.settings<Gfx::Settings::Model>().getRate();

  // Timer for graph update
  if(m_no_vsync_timer == nullptr) {
    m_no_vsync_timer = m_timers.acquireTimer(this, rate);
    connect(m_no_vsync_timer, &score::HighResolutionTimer::timeout, this, &GfxContext::on_no_vsync_timer, Qt::UniqueConnection);
  }

  // Render is done in the widget
}

void GfxContext::recompute_connections()
{
  recompute_graph();
}

void GfxContext::incrementalEdgeUpdate(
    const ossia::flat_set<EdgeSpec>& old_edges,
    const ossia::flat_set<EdgeSpec>& cur_edges)
{
  // Compute diff
  std::vector<EdgeSpec> removed;
  std::vector<EdgeSpec> added;

  std::set_difference(
      old_edges.begin(), old_edges.end(),
      cur_edges.begin(), cur_edges.end(),
      std::back_inserter(removed));

  std::set_difference(
      cur_edges.begin(), cur_edges.end(),
      old_edges.begin(), old_edges.end(),
      std::back_inserter(added));

  // Pre-compute the set of sink ports that will be fed by an incoming edge
  // in this same batch. Handing that set to onEdgeRemoved prevents the
  // "remove A→B, add F→B" sequence from destroying B's input RT in the
  // gap between the two, which was pure churn when the old and new feeds
  // share a sink port (classic filter insertion). Reconcile reallocates
  // RTs only when the slot is empty, so preserving the existing RT lets
  // the new pass slot straight into place. Source: Graph.cpp
  // createPassForEdgeIfMissing already treats a present RT as valid
  // regardless of the edge that produced it.
  ossia::hash_set<const score::gfx::Port*> preserveSinks;
  preserveSinks.reserve(added.size());
  for(auto& spec : added)
  {
    auto sink_it = nodes.find(spec.second.node);
    if(sink_it == nodes.end())
      continue;
    // EdgeSpecs are script-supplied: guard against null nodes and
    // out-of-range port indices before indexing, exactly as
    // add_edge/remove_edge do. An OOB std::vector access is UB, not a
    // catchable exception, so the try/catch around the caller cannot
    // save us here.
    if(!sink_it->second)
      continue;
    auto& sink_ports = sink_it->second->input;
    if(spec.second.port >= sink_ports.size())
      continue;
    preserveSinks.insert(sink_ports[spec.second.port]);
  }

  // Process removals first (while edge objects still exist).
  for(auto& spec : removed)
  {
    auto source_it = nodes.find(spec.first.node);
    auto sink_it = nodes.find(spec.second.node);
    if(source_it == nodes.end() || sink_it == nodes.end())
      continue;
    if(!source_it->second || !sink_it->second)
      continue;

    auto& source_ports = source_it->second->output;
    auto& sink_ports = sink_it->second->input;
    if(spec.first.port >= source_ports.size()
       || spec.second.port >= sink_ports.size())
      continue;

    auto* source_port = source_ports[spec.first.port];
    auto* sink_port = sink_ports[spec.second.port];

    // Find the actual Edge object
    score::gfx::Edge* edge = nullptr;
    for(auto* e : source_port->edges)
    {
      if(e->sink == sink_port)
      {
        edge = e;
        break;
      }
    }

    if(edge)
    {
      // Notify graph BEFORE destroying the edge
      m_graph->onEdgeRemoved(*edge, &preserveSinks);
      m_graph->removeEdge(source_port, sink_port);
    }
  }

  // Process additions: first create all edge objects in the graph,
  // then reconcile render lists in one pass. Processing edges one
  // at a time doesn't work because edge ordering creates dependencies
  // (e.g. edge A->B is skipped because B isn't in the RL yet, then
  // edge B->C brings B into the RL, but A never gets a renderer).
  // Edges whose endpoint node is not present YET (its ADD_NODE command has
  // not been dequeued when this edge diff runs — the two channels are
  // independent). These must NOT be treated as applied: updateGraph already
  // committed cur_edges to the authoritative `edges` baseline, so unless we
  // roll them back the next diff sees old_edges == cur_edges for them and
  // never re-emits them — the connection is lost forever until an unrelated
  // full rebuild. We drop them from the baseline and re-raise edges_changed
  // so the next tick (by which the node has been added) re-emits and wires
  // them.
  std::vector<EdgeSpec> deferred;
  for(auto& spec : added)
  {
    auto source_it = nodes.find(spec.first.node);
    auto sink_it = nodes.find(spec.second.node);
    if(source_it == nodes.end() || sink_it == nodes.end())
    {
      deferred.push_back(spec);
      continue;
    }
    if(!source_it->second || !sink_it->second)
    {
      deferred.push_back(spec);
      continue;
    }

    auto& source_ports = source_it->second->output;
    auto& sink_ports = sink_it->second->input;
    if(spec.first.port >= source_ports.size()
       || spec.second.port >= sink_ports.size())
      continue;

    auto* source_port = source_ports[spec.first.port];
    auto* sink_port = sink_ports[spec.second.port];

    m_graph->addEdge(source_port, sink_port, spec.type);
  }

  if(!deferred.empty())
  {
    std::lock_guard l{edges_lock};
    for(const auto& spec : deferred)
      edges.erase(spec);
    // Force updateGraph to re-enter the edge-diff path next tick even if the
    // producer does not republish new_edges; old_edges will then lack the
    // deferred edges so set_difference re-emits them once their node exists.
    edges_changed.store(true);
  }

  // Reconcile: ensure all reachable nodes have renderers and passes.
  // This handles NEW nodes (creates renderers + passes for all their edges).
  if(!added.empty() || !removed.empty())
    m_graph->reconcileAllRenderLists();

  // Create missing passes and update samplers for ALL edges in the graph,
  // not just the newly-added ones. When a node becomes reachable through a
  // new edge (e.g. filter→Grid makes filter reachable), pre-existing edges
  // TO that node (e.g. A→filter) also need passes created. Checking only
  // the diff misses these.
  m_graph->createAllMissingPasses();
  m_graph->updateAllSinkSamplers();
}

void GfxContext::update_inputs()
{
  score::gfx::Message msg;
  while(tick_messages.try_dequeue(msg))
  {
    if(auto it = nodes.find(msg.node_id); it != nodes.end())
    {
      auto& node = it->second;
      node->process(std::move(msg));
    }

    if(msg.input.capacity() > 0)
      m_buffers.release(std::move(msg).input);
  }

  for(auto& n : nodes)
  {
    n.second->update();
  }
}

void GfxContext::remove_node(
    std::vector<std::unique_ptr<score::gfx::Node>>& nursery, int32_t index)
{
  // Remove all edges involving that node. recompute_edges snapshots
  // `edges` under edges_lock, so take it here too while mutating.
  {
    std::lock_guard l{edges_lock};
    for(auto it = this->edges.begin(); it != this->edges.end();)
    {
      if(it->first.node == index || it->second.node == index)
        it = this->edges.erase(it);
      else
        ++it;
    }
  }

  if(auto node_it = nodes.find(index); node_it != nodes.end())
  {
    auto node = node_it->second.get();

    // Remove the node from the render clocks if it's in there. An emptied
    // TimerClock is dropped; its dtor releases the shared timer back to the
    // pool (same as the old releaseTimer path).
    for(auto it = m_renderClocks.begin(); it != m_renderClocks.end();)
    {
      (*it)->removeOutput((score::gfx::OutputNode*)node);

      if((*it)->empty())
        it = m_renderClocks.erase(it);
      else
        ++it;
    }

    m_graph->removeNode(node);

    // Needed because when removing edges in recompute_graph,
    // they remove themselves from the nodes / ports in their dtor
    // thus if the items are deleted before that, it would crash
    nursery.push_back(std::move(node_it->second));

    nodes.erase(node_it);
  }
}

void GfxContext::run_commands()
{
  std::vector<std::unique_ptr<score::gfx::Node>> nursery;

  bool recompute = false;
  std::vector<score::gfx::Node*> add_output;
  Command c = NodeCommand{};
  while(tick_commands.try_dequeue(c))
  {
    if(auto cnode = ossia::get_if<NodeCommand>(&c))
    {
      auto& cmd = *cnode;
      switch(cmd.cmd)
      {
        case NodeCommand::ADD_PREVIEW_NODE: {
          m_graph->addNode(cmd.node.get());
          add_output.push_back(cmd.node.get());
          nodes[cmd.index] = {std::move(cmd.node)};
          break;
        }
        case NodeCommand::ADD_NODE: {
          m_graph->addNode(cmd.node.get());
          nodes[cmd.index] = {std::move(cmd.node)};
          // Only output nodes require a full rebuild (new window/timer).
          // Non-output nodes just wait for edges — the incremental
          // reconciliation path creates their renderers when connected.
          if(dynamic_cast<score::gfx::OutputNode*>(nodes[cmd.index].get()))
            recompute = true;
          break;
        }
        case NodeCommand::REMOVE_PREVIEW_NODE: {
          auto& node = nodes.at(cmd.index);
          auto n = dynamic_cast<score::gfx::OutputNode*>(node.get());
          SCORE_ASSERT(n);
          {
            // recompute_edges snapshots preview_edges under edges_lock,
            // so guard reads/mutations of it here too. remove_edge only
            // touches m_graph, so keep it outside the lock.
            EdgeSpec to_remove;
            bool found = false;
            {
              std::lock_guard l{edges_lock};
              auto it = ossia::find_if(this->preview_edges, [idx = cmd.index](EdgeSpec e) {
                return e.second.node == idx;
              });
              if(it != this->preview_edges.end())
              {
                to_remove = *it;
                found = true;
              }
            }
            if(found)
            {
              this->remove_edge(to_remove);
              std::lock_guard l{edges_lock};
              this->preview_edges.erase(to_remove);
            }
          }
          m_graph->destroyOutputRenderList(*n);
          remove_node(nursery, cmd.index);
          break;
        }
        case NodeCommand::REMOVE_NODE: {
          if(auto node_it = nodes.find(cmd.index); node_it != nodes.end())
          {
            bool is_output = dynamic_cast<score::gfx::OutputNode*>(node_it->second.get());
            if(!is_output)
            {
              // Incremental removal: clean up edges, renderers, retopo sort.
              // Must happen BEFORE remove_node deletes the node.
              m_graph->removeNodeAndEdges(node_it->second.get());
            }
            remove_node(nursery, cmd.index);
            if(is_output)
            {
              // Recompute immediately so subsequent commands in this tick
              // see a consistent graph state. Deferring until the end of
              // the loop leaves the graph half-broken (node gone from
              // m_nodes but renderer/output still wired) for any further
              // commands or render frames that fire in this window.
              recompute_graph();
              m_fullRebuildThisFrame = true;
            }
          }
          break;
        }
        case NodeCommand::RELINK: {
          recompute = true;
          break;
        }
      }
    }
    else if(auto cedge = ossia::get_if<EdgeCommand>(&c))
    {
      auto& cmd = *cedge;
      switch(cmd.cmd)
      {
        case EdgeCommand::CONNECT_PREVIEW_NODE: {
          {
            std::lock_guard l{edges_lock};
            this->preview_edges.emplace(cmd.edge);
          }
          add_edge(cmd.edge);
          break;
        }
        case EdgeCommand::DISCONNECT_PREVIEW_NODE: {
          {
            std::lock_guard l{edges_lock};
            this->preview_edges.erase(cmd.edge);
          }
          remove_edge(cmd.edge);
          break;
        }
      }
    }
  }

  for(auto& node : nursery)
  {
    ossia::remove_erase(add_output, node.get());
  }
  for(auto* out : add_output)
    add_preview_output(*safe_cast<score::gfx::OutputNode*>(out));
  if(recompute)
  {
    recompute_graph();
    // Signal to updateGraph() that a full rebuild happened this frame.
    // The incremental edge path should NOT run after a full rebuild,
    // because the graph was just rebuilt with the old edge set and
    // applying an incremental diff would result in a half-built state.
    m_fullRebuildThisFrame = true;
  }

  // This will force the nodes to be deleted in the main thread a bit later
  // as for some reason when the ScreenNode is deleted, it still gets rendered to...
  if(!nursery.empty())
  {
    QTimer::singleShot(
        100, qApp, [nodes = std::move(nursery)]() mutable { nodes.clear(); });
    nursery.clear();
  }
}

void GfxContext::updateGraph()
{
  run_commands();

  update_inputs();

  // Clear the flag BEFORE copying new_edges so a producer that publishes a
  // fresh edge set after our copy (and re-sets the flag) cannot have its
  // signal lost: the worst case is one redundant reprocess next tick, never
  // a dropped update. Clearing it after the copy (the previous behaviour)
  // could clobber a set-after-copy and, with prev_edges dedup on the
  // producer side, that update would never be re-sent.
  if(edges_changed.exchange(false))
  {
    ossia::flat_set<EdgeSpec> old_edges;
    ossia::flat_set<EdgeSpec> cur_edges;
    {
      std::lock_guard l{edges_lock};
      old_edges = edges;
      edges = new_edges;
      cur_edges = edges;
    }

    // If a full rebuild happened this frame (nodes added/removed),
    // use the nuclear path for edges too. The incremental path
    // doesn't work correctly after a full rebuild because the graph
    // was rebuilt with the old edge set.
    if(m_fullRebuildThisFrame)
    {
      m_fullRebuildThisFrame = false;
      recompute_connections();
      return;
    }
    // Incremental edge update: apply the diff between old and new edges.
    try
    {
      incrementalEdgeUpdate(old_edges, cur_edges);
    }
    catch(const std::exception& e)
    {
      qWarning("Incremental edge update failed (%s), falling back to full rebuild",
               e.what());
      recompute_connections();
    }
    catch(...)
    {
      qWarning("Incremental edge update failed, falling back to full rebuild");
      recompute_connections();
    }
  }
}

void GfxContext::on_no_vsync_timer(score::HighResolutionTimer* self)
{
  updateGraph();
}

void GfxContext::on_watchdog_timer(score::HighResolutionTimer* self)
{
  if(m_renderClocks.empty() && !m_no_vsync_timer)
    updateGraph();
}
}

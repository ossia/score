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
  if(source_node_it != this->nodes.end())
  {
    auto sink_node_it = this->nodes.find(edge.second.node);
    if(sink_node_it != this->nodes.end())
    {
      assert(source_node_it->second);
      assert(sink_node_it->second);

      auto& source_ports = source_node_it->second->output;
      auto& sink_ports = sink_node_it->second->input;

      SCORE_ASSERT(source_ports.size() > 0);
      SCORE_ASSERT(sink_ports.size() > 0);
      SCORE_ASSERT(source_ports.size() > edge.first.port);
      SCORE_ASSERT(sink_ports.size() > edge.second.port);
      auto source_port = source_ports[edge.first.port];
      auto sink_port = sink_ports[edge.second.port];

      m_graph->addEdge(source_port, sink_port);
    }
  }
}

void GfxContext::remove_edge(EdgeSpec edge)
{
  auto source_node_it = this->nodes.find(edge.first.node);
  if(source_node_it != this->nodes.end())
  {
    auto sink_node_it = this->nodes.find(edge.second.node);
    if(sink_node_it != this->nodes.end())
    {
      assert(source_node_it->second);
      assert(sink_node_it->second);

      auto source_port = source_node_it->second->output[edge.first.port];
      auto sink_port = sink_node_it->second->input[edge.second.port];

      m_graph->removeEdge(source_port, sink_port);
    }
  }
}

void GfxContext::recompute_edges()
{
  m_graph->clearEdges();

  for(auto edge : edges)
  {
    add_edge(edge);
  }
  for(auto edge : preview_edges)
  {
    add_edge(edge);
  }
}

void GfxContext::recompute_graph()
{
  // Clear previous timers
  std::destroy_at(&m_timers);
  std::construct_at(&m_timers);
  {
    m_watchdog_timer = m_timers.acquireTimer(this, 20.);
    connect(m_watchdog_timer, &score::HighResolutionTimer::timeout, this, &GfxContext::on_watchdog_timer, Qt::UniqueConnection);
  }
  m_no_vsync_timer = nullptr;
  m_manualTimers.clear();

  for(auto& output : m_graph->outputs())
  {
    output->setVSyncCallback({});
  }

  // Recreate the graph
  recompute_edges();

  auto& settings = m_context.app.settings<Gfx::Settings::Model>();
  const double settings_rate = m_context.app.settings<Gfx::Settings::Model>().getRate();
  const auto api = settings.graphicsApiEnum();

  m_graph->createAllRenderLists(api);

  // Recreate new timers
  const bool vsync = settings.getVSync() && m_graph->canDoVSync();

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
    m_graph->outputs().front()->setVSyncCallback([this] { updateGraph(); });
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


    // This starts the timers which control the actual render rate of various things
    for(auto& output : m_graph->outputs())
    {
      auto conf = output->configuration();
      if(conf.manualRenderingRate)
      {
        bool existing_timer{};
        for(auto& tm : m_manualTimers)
        {
          if(tm.first->frequency() == 1000. / *conf.manualRenderingRate)
          {
            tm.second.insert(output);
            existing_timer = true;
            break;
          }
        }

        if(!existing_timer)
        {
          auto id = m_timers.acquireTimer(this, 1000. / *conf.manualRenderingRate);
          m_manualTimers[id].insert(output);
          connect(id, &score::HighResolutionTimer::timeout, this, &GfxContext::on_manual_timer, Qt::QueuedConnection);
        }
      }
    }
  }
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
  // FIXME for more performance
  /*
  recompute_edges();
  // m_graph->setupOutputs(m_api);
  m_graph->relinkGraph();
  */
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
  // Remove all edges involving that node
  for(auto it = this->edges.begin(); it != this->edges.end();)
  {
    if(it->first.node == index || it->second.node == index)
      it = this->edges.erase(it);
    else
      ++it;
  }

  if(auto node_it = nodes.find(index); node_it != nodes.end())
  {
    auto node = node_it->second.get();

    // Remove the node from the timers if it's in there
    for(auto timer_it = m_manualTimers.begin(); timer_it != m_manualTimers.end();)
    {
      auto& nodes = timer_it->second;
      nodes.erase((score::gfx::OutputNode*)node);

      if(nodes.empty())
      {
        m_timers.releaseTimer(this, timer_it->first);
        timer_it = m_manualTimers.erase(timer_it);
      }
      else
      {
        ++timer_it;
      }
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
          recompute = true;
          break;
        }
        case NodeCommand::REMOVE_PREVIEW_NODE: {
          auto& node = nodes.at(cmd.index);
          auto n = dynamic_cast<score::gfx::OutputNode*>(node.get());
          SCORE_ASSERT(n);
          {
            auto it = ossia::find_if(this->preview_edges, [idx = cmd.index](EdgeSpec e) {
              return e.second.node == idx;
            });
            if(it != this->preview_edges.end())
            {
              this->remove_edge(*it);
              this->preview_edges.erase(*it);
            }
          }
          m_graph->destroyOutputRenderList(*n);
          remove_node(nursery, cmd.index);
          break;
        }
        case NodeCommand::REMOVE_NODE: {
          remove_node(nursery, cmd.index);
          recompute = true;
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
          this->preview_edges.emplace(cmd.edge);
          add_edge(cmd.edge);
          break;
        }
        case EdgeCommand::DISCONNECT_PREVIEW_NODE: {
          this->preview_edges.erase(cmd.edge);
          remove_edge(cmd.edge);
          break;
        }
      }
    }
  }

  for(auto* out : add_output)
    add_preview_output(*safe_cast<score::gfx::OutputNode*>(out));
  if(recompute)
  {
    recompute_graph();
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

  if(edges_changed)
  {
    {
      std::lock_guard l{edges_lock};
      std::swap(edges, new_edges);
    }
    recompute_connections();
    edges_changed = false;
  }
}

void GfxContext::on_no_vsync_timer(score::HighResolutionTimer* self)
{
  updateGraph();
}

void GfxContext::on_watchdog_timer(score::HighResolutionTimer* self)
{
  if(m_manualTimers.empty() && !m_no_vsync_timer)
    updateGraph();
}

void GfxContext::on_manual_timer(score::HighResolutionTimer* self)
{
  if(auto ptr = m_manualTimers.find(self); ptr != m_manualTimers.end())
  {
    for(auto output : ptr->second) {
      output->render();
    }
  }
}
}

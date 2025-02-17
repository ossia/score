#include "ISFNode.hpp"

#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Window.hpp>

#include <score/gfx/Vulkan.hpp>
#include <score/tools/Debug.hpp>

#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/ssize.hpp>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>

namespace score::gfx
{
template <typename Graph_T, typename IO>
void print_graph(Graph_T& g, IO& stream)
{
#if 0
  std::stringstream s;
  boost::write_graphviz(
      s, g,
      [&](auto& out, auto v) {
    if(g[v])
    {

      out << "[label=\"";
      auto n = g[v];
      if(auto i = dynamic_cast<ISFNode*>(n))
        out << i->m_descriptor.description;
      else
        out << "output";
      out << "\"]";
    }
    else
      out << "[]";
      },
      [](auto&&...) {});

  stream << s.str() << "\n";
#endif
}

using Vertex = score::gfx::Node*;
using GraphImpl
    = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, Vertex>;
using VertexMap = ossia::hash_map<score::gfx::Node*, GraphImpl::vertex_descriptor>;
static void graphwalk(
    score::gfx::Node* node, std::vector<score::gfx::Node*>& list, GraphImpl& g,
    VertexMap& m)
{
  auto sink_desc = m[node];
  for(auto inputs : node->input)
  {
    for(auto edge : inputs->edges)
    {
      if(!edge->source->node->addedToGraph)
      {
        list.push_back(edge->source->node);

        auto src_desc = boost::add_vertex(edge->source->node, g);
        m[edge->source->node] = src_desc;
        edge->source->node->addedToGraph = true;
        boost::add_edge(src_desc, sink_desc, g);
      }
      else
      {
        auto src_desc = m[edge->source->node];
        boost::add_edge(src_desc, sink_desc, g);
      }
    }
  }
}

static void graphwalk(std::vector<score::gfx::Node*>& model_nodes)
{
  GraphImpl g;
  VertexMap m;
  auto k = boost::add_vertex(model_nodes.front(), g);
  m[model_nodes.front()] = k;

  std::size_t processed = 0;
  while(processed != model_nodes.size())
  {
    graphwalk(model_nodes[processed], model_nodes, g, m);
    processed++;
  }

  ossia::int_vector topo_order;
  topo_order.reserve(model_nodes.size());

  try
  {
    model_nodes.clear();
    boost::topological_sort(g, std::back_inserter(topo_order));
    for(auto it = topo_order.begin(); it != topo_order.end(); ++it)
    {
      auto e = *it;
      SCORE_ASSERT(g[e]);
      model_nodes.push_back(g[e]);
    }
  }
  catch(const std::exception& e)
  {
    qDebug() << "Invalid gfx graph: " << e.what();
  }
}

void Graph::createAllRenderLists(GraphicsApi graphicsApi)
{
#if QT_HAS_VULKAN
  if(graphicsApi == Vulkan)
  {
    if(!staticVulkanInstance())
    {
      qWarning("Failed to create Vulkan instance, switching to OpenGL");
      graphicsApi = OpenGL;
    }
  }
#endif

  for(auto output : m_outputs)
  {
    output->stopRendering();
  }

  for(auto node : m_nodes)
  {
    node->renderedNodes.clear();
    node->renderedNodesChanged();
  }

  for(auto& renderer : m_renderers)
  {
    renderer->release();
  }

  m_renderers.clear();
  m_outputs.clear();

  ossia::flat_set<OutputNode*> parent_nodes;
  for(auto node : m_nodes)
  {
    if(auto out = dynamic_cast<OutputNode*>(node))
    {
      m_outputs.push_back(out);
      if(auto ptr = out->configuration().parent)
        parent_nodes.insert(ptr);
    }
  }

  // For multi-viewport renders
  for(auto node : parent_nodes)
  {
    if(!ossia::contains(m_outputs, node))
    {
      m_outputs.push_back(node);
    }
  }

  m_renderers.reserve(ossia::max(16, std::ssize(m_outputs)));

  for(auto output : m_outputs)
  {
    initializeOutput(output, graphicsApi);
    output->startRendering();
  }
}

void Graph::createSingleRenderList(
    score::gfx::OutputNode& output, GraphicsApi graphicsApi)
{
#if QT_HAS_VULKAN
  if(graphicsApi == Vulkan)
  {
    if(!staticVulkanInstance())
    {
      qWarning("Failed to create Vulkan instance, switching to OpenGL");
      graphicsApi = OpenGL;
    }
  }
#endif

  initializeOutput(&output, graphicsApi);
  output.startRendering();
}

void Graph::createOutputRenderList(OutputNode& output)
{
  if(output.renderState())
  {
    if(auto rl = createRenderList(&output, output.renderState()))
      m_renderers.push_back(std::move(rl));
  }
}

void Graph::recreateOutputRenderList(OutputNode& output)
{
  auto it = ossia::find_if(
      m_renderers, [rend = output.renderer()](const std::shared_ptr<RenderList>& r) {
        return r.get() == rend;
      });

  if(it != m_renderers.end())
  {
    std::shared_ptr<RenderList>& renderer = *it;
    if(renderer.get() == output.renderer())
    {
      auto old_renderer = renderer;
      auto new_renderer = createRenderList(&output, output.renderState());

      old_renderer->release();

      renderer = new_renderer;

      old_renderer.reset();

      if(!renderer)
      {
        output.setRenderer({});
        it = m_renderers.erase(it);
      }
    }
    else
    {
      qDebug("???");
    }
  }
}

void Graph::initializeOutput(OutputNode* output, GraphicsApi graphicsApi)
{
  output->updateGraphicsAPI(graphicsApi);
  if(!output->canRender() || !output->renderState())
  {
    auto onReady = [this, output] {
      if(output->canRender())
        createOutputRenderList(*output);
    };

    auto onResize = [this, output] { recreateOutputRenderList(*output); };

    auto onUpdate = [this] {
      switch(this->m_outputs.size())
      {
        case 1:
          if(this->m_vsync_callback)
            this->m_vsync_callback();
          break;
        default:
          break;
      }
    };

    // TODO only works for one output !!
    output->createOutput(graphicsApi, onReady, onUpdate, onResize);
  }
  else
  {
    createOutputRenderList(*output);
    // output->window->state.hasSwapChain = true;
  }
}

void Graph::relinkGraph()
{
  for(auto r_it = m_renderers.begin(); r_it != m_renderers.end();)
  {
    auto& r = **r_it;
    for(auto& node : m_nodes)
      node->addedToGraph = false;

    assert(!r.nodes.empty());

    auto out = r.nodes.back();
    r.nodes.clear();
    r.nodes.push_back(out);

    r.clearRenderers();

    auto& model_nodes = r.nodes;
    {
      // In which order do we want to render stuff
      graphwalk(model_nodes);

      if(model_nodes.size() > 1)
      {
        bool invalid_renderlist = false;
        for(auto node : model_nodes)
        {
          score::gfx::NodeRenderer* rn{};
          auto it = node->renderedNodes.find(&r);
          if(it == node->renderedNodes.end())
          {
            if((rn = node->createRenderer(r)))
            {
              rn->id = node->id;
              node->renderedNodes.emplace(&r, rn);

              node->renderedNodesChanged();
              //rn->init(r);
            }
            else
            {
              invalid_renderlist = true;
              break;
            }
          }
          else
          {
            rn = it->second;
            SCORE_ASSERT(rn);
            rn->release(r);
            //rn->init(r);
          }
          SCORE_ASSERT(rn);
          r.renderers.push_back(rn);
        }

        // If a node couldn't be recreated, we skip the whole thing
        if(invalid_renderlist)
        {
          r.output.setRenderer({});
          r_it = m_renderers.erase(r_it);
          break;
        }

        //         for(auto node : r.renderers)
        //         {
        //           node->init(r);
        //         }
      }
      else if(model_nodes.size() == 1)
      {
        SCORE_ASSERT(
            model_nodes[0]->renderedNodes.find(&r)
            != model_nodes[0]->renderedNodes.end());
        auto rn = model_nodes[0]->renderedNodes.find(&r)->second;
        SCORE_ASSERT(rn);
        rn->release(r);
      }
    }
    r.output.onRendererChange();

    ++r_it;
  }

  if(m_outputs.size() > m_renderers.size())
  {
    // Try to recreate missing ones
    for(auto& output : m_outputs)
    {
      if(!output->renderer() && output->canRender())
      {
        createOutputRenderList(*output);
      }
    }
  }
}

void Graph::setVSyncCallback(std::function<void()> cb)
{
  // TODO thread safety if vulkan uses a thread ?
  // If we have more than one output, then instead we sync them with
  // a simple timer, as they may have drastically different vsync rates.
  m_vsync_callback = cb;
}

bool Graph::canDoVSync() const noexcept
{
  return m_outputs.size() == 1
         && !m_outputs[0]->configuration().manualRenderingRate.has_value();
}

static bool createNodeRenderer(score::gfx::Node& node, RenderList& r)
{
  // Register the node with the renderer
  if(auto rn = node.createRenderer(r))
  {
    rn->id = node.id;
    r.renderers.push_back(rn);

    // Register the rendered nodes with their parents
    SCORE_ASSERT(node.renderedNodes.find(&r) == node.renderedNodes.end());
    node.renderedNodes.emplace(&r, rn);
    node.renderedNodesChanged();
    return true;
  }

  return false;
}

std::shared_ptr<RenderList>
Graph::createRenderList(OutputNode* output, std::shared_ptr<RenderState> state)
{
  auto ptr = std::make_shared<RenderList>(*output, state);
  output->setRenderer(ptr);
  for(auto& node : m_nodes)
    node->addedToGraph = false;

  RenderList& r = *ptr;
  auto& model_nodes = r.nodes;
  {
    model_nodes.push_back(output);

    // In which order do we want to render stuff
    graphwalk(model_nodes);

    // Now we have the nodes in the order in which they are going to
    // be init'd (e.g. output node first to create the render targets)
    // We create renderers for each of them
    for(auto node : model_nodes)
    {
      if(!createNodeRenderer(*node, r))
      {
        output->setRenderer(nullptr);
        return {};
      }
    }
  }

  output->onRendererChange();
  {
    r.init();

    if(model_nodes.size() > 1)
    {
      auto batch = r.initialBatch();
      for(auto node : r.renderers)
        node->init(r, *batch);
    }
  }

  return ptr;
}

Graph::Graph() { }

Graph::~Graph()
{
  for(auto& renderer : m_renderers)
  {
    renderer->release();
  }

  for(auto out : m_outputs)
  {
    out->destroyOutput();
  }

  clearEdges();
}

void Graph::addNode(Node* n)
{
  m_nodes.push_back(n);
}

void Graph::removeNode(Node* n)
{
  ossia::remove_erase(m_nodes, n);
}

void Graph::clearEdges()
{
  for(auto edge : m_edges)
  {
    delete edge;
  }
  m_edges.clear();
}

void Graph::addEdge(Port* source, Port* sink)
{
  auto it = ossia::find_if(
      m_edges, [=](Edge* e) { return e->source == source && e->sink == sink; });

  if(it == m_edges.end())
  {
    m_edges.push_back(new Edge{source, sink});
  }
#if defined(SCORE_DEBUG)
  else
  {
    qDebug() << "Tried to add edge between " << source << sink << "\n   ==> "
             << typeid(*source->node).name() << typeid(*sink->node).name();
  }
#endif
}

void Graph::removeEdge(Port* source, Port* sink)
{
  auto it = ossia::find_if(
      m_edges, [=](Edge* e) { return e->source == source && e->sink == sink; });
  if(it != m_edges.end())
  {
    delete *it;
    m_edges.erase(it);
  }
}

void Graph::addAndLinkEdge(Port* source, Port* sink)
{
  addEdge(source, sink);

  auto output = dynamic_cast<OutputNode*>(sink->node);
  SCORE_ASSERT(output);

  recreateOutputRenderList(*output);
}

void Graph::unlinkAndRemoveEdge(Port* source, Port* sink)
{
  removeEdge(source, sink);
  auto output = dynamic_cast<OutputNode*>(sink->node);
  SCORE_ASSERT(output);

  recreateOutputRenderList(*output);
}

void Graph::destroyOutputRenderList(score::gfx::OutputNode& output)
{
  auto it = ossia::find_if(
      m_renderers, [rend = output.renderer()](const std::shared_ptr<RenderList>& r) {
        return r.get() == rend;
      });

  if(it != m_renderers.end())
  {
    std::shared_ptr<RenderList>& renderer = *it;
    if(renderer.get() == output.renderer())
    {
      renderer->release();
      renderer.reset();

      output.setRenderer({});
      it = m_renderers.erase(it);
    }
    else
    {
      qDebug("???");
    }
  }

  ossia::remove_erase(m_outputs, &output);
}

}

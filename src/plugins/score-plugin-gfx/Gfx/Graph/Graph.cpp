#include "ISFNode.hpp"
#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Window.hpp>

#include <score/gfx/Vulkan.hpp>
#include <score/tools/Debug.hpp>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/graphviz.hpp>

namespace score::gfx
{
template <typename Graph_T, typename IO>
void print_graph(Graph_T& g, IO& stream)
{
  std::stringstream s;
  boost::write_graphviz(
      s, g, [&](auto& out, auto v) {
        if (g[v])
          {

            out << "[label=\"";
            auto n = g[v];
            if(auto i = dynamic_cast<ISFNode*>(n))out << i->m_descriptor.description;
            else out << "output";
            out<< "\"]";
          }
        else
          out << "[]";
      },
      [](auto&&...) {});

  stream << s.str() << "\n";
}

using Vertex = score::gfx::Node*;
using GraphImpl = boost::
    adjacency_list<boost::vecS, boost::vecS, boost::directedS, Vertex>;
using VertexMap = std::map<score::gfx::Node*, GraphImpl::vertex_descriptor>;
static void
graphwalk(score::gfx::Node* node, std::vector<score::gfx::Node*>& list, GraphImpl& g, VertexMap& m)
{
  auto sink_desc = m[node];
  for (auto inputs : node->input)
  {
    for (auto edge : inputs->edges)
    {
      if (!edge->source->node->addedToGraph)
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

  int processed = 0;
  while (processed != model_nodes.size())
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
    for (auto it = topo_order.begin(); it != topo_order.end(); ++it)
    {
      auto e = *it;
      SCORE_ASSERT(g[e]);
      model_nodes.push_back(g[e]);
    }
  }
  catch (const std::exception& e)
  {
    qDebug() << "Invalid gfx graph: " << e.what();
  }
}

void Graph::createAllRenderLists(GraphicsApi graphicsApi)
{
#if QT_CONFIG(vulkan)
  if (graphicsApi == Vulkan)
  {
    if (!staticVulkanInstance())
    {
      qWarning("Failed to create Vulkan instance, switching to OpenGL");
      graphicsApi = OpenGL;
    }
  }
#endif

  for (auto output : m_outputs)
  {
    output->stopRendering();
  }

  for (auto node : m_nodes)
  {
    node->renderedNodes.clear();
  }

  for (auto& renderer : m_renderers)
  {
    renderer->release();
  }

  m_renderers.clear();
  m_outputs.clear();

  for (auto node : m_nodes)
    if (auto out = dynamic_cast<OutputNode*>(node))
      m_outputs.push_back(out);

  m_renderers.reserve(std::max((int)16, (int)m_outputs.size()));

  for (auto output : m_outputs)
  {
    output->updateGraphicsAPI(graphicsApi);
    if (!output->canRender())
    {
      auto onReady = [=] {
        if(output->canRender())
        m_renderers.push_back(createRenderList(output, *output->renderState()));
      };
      auto onResize = [=] {
        for (std::shared_ptr<RenderList>& renderer : this->m_renderers)
        {
          if (renderer.get() == output->renderer())
          {
            auto old_renderer = renderer;
            auto new_renderer = createRenderList(output, *output->renderState());

            old_renderer->release();

            renderer = new_renderer;

            old_renderer.reset();
            break;
          }
        }
      };

      auto onUpdate = [this] {
        switch (this->m_outputs.size())
        {
          case 1:
            if (this->m_vsync_callback)
              this->m_vsync_callback();
            break;
          default:
            break;
        }
      };
      // TODO only works for one output !!
      output->createOutput(
          graphicsApi,
          onReady,
          onUpdate,
          onResize);
    }
    else
    {
      if (auto rs = output->renderState())
      {
        m_renderers.push_back(createRenderList(output, *rs));
      }
      // output->window->state.hasSwapChain = true;
    }

    output->startRendering();
  }
}

void Graph::relinkGraph()
{
  for (auto& rptr : m_renderers)
  {
    auto& r = *rptr;
    for (auto& node : m_nodes)
      node->addedToGraph = false;

    assert(!r.nodes.empty());

    auto out = r.nodes.back();
    r.nodes.clear();
    r.nodes.push_back(out);

    r.renderers.clear();

    auto& model_nodes = r.nodes;
    {
      // In which order do we want to render stuff
      graphwalk(model_nodes);

      if (model_nodes.size() > 1)
      {
        for (auto node : model_nodes)
        {
          auto rn = node->renderedNodes[&r];
          if (!rn)
          {
            rn = node->createRenderer(r);
            SCORE_ASSERT(rn);
            node->renderedNodes[&r] = rn;
            //rn->init(r);
          }
          else
          {
            rn->release(r);
            //rn->init(r);
          }
          SCORE_ASSERT(rn);
          r.renderers.push_back(rn);
        }

        for (auto node : r.renderers)
        {
          node->init(r);
        }
      }
      else if (model_nodes.size() == 1)
      {
        auto rn = model_nodes[0]->renderedNodes[&r];
        assert(rn);
        rn->release(r);
      }
    }
    r.output.onRendererChange();
  }
}

void Graph::setVSyncCallback(std::function<void()> cb)
{
  // TODO thread safety if vulkan uses a thread ?
  // If we have more than one output, then instead we sync them with
  // a simple timer, as they may have drastically different vsync rates.
  m_vsync_callback = cb;
}

static void createNodeRenderer(score::gfx::Node& node, RenderList& r)
{
  auto rn = node.createRenderer(r);

  // Register the node with the renderer
  r.renderers.push_back(rn);

  // Register the rendered nodes with their parents
  node.renderedNodes[&r] = rn;
}

std::shared_ptr<RenderList>
Graph::createRenderList(OutputNode* output, RenderState state)
{
  auto ptr = std::make_shared<RenderList>(*output, state);
  for (auto& node : m_nodes)
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
    for (auto node : model_nodes)
    {
      createNodeRenderer(*node, r);
    }
  }

  output->onRendererChange();
  {
    r.init();

    if (model_nodes.size() > 1)
    {
      for (auto node : r.renderers)
        node->init(r);
    }
  }

  return ptr;
}

Graph::Graph() {

}

Graph::~Graph()
{
  for (auto& renderer : m_renderers)
  {
    renderer->release();
  }

  for (auto out : m_outputs)
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
  for (auto edge : m_edges)
  {
    delete edge;
  }
  m_edges.clear();
}

void Graph::addEdge(Port* source, Port* sink)
{
  m_edges.push_back(new Edge{source, sink});
}
}

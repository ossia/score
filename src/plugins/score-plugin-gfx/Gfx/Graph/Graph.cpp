#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Window.hpp>

#include <score/gfx/Vulkan.hpp>
#include <score/tools/Debug.hpp>

namespace score::gfx
{
static void
graphwalk(score::gfx::Node* node, std::vector<score::gfx::Node*>& list)
{
  for (auto inputs : node->input)
  {
    for (auto edge : inputs->edges)
    {
      if (!edge->source->node->addedToGraph)
      {
        list.push_back(edge->source->node);
        edge->source->node->addedToGraph = true;
      }
    }
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

    for (auto rn : renderer->renderedNodes)
      delete rn;
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
        m_renderers.push_back(createRenderList(output, *output->renderState()));
      };
      auto onResize = [=] {
        for (std::shared_ptr<RenderList>& renderer : this->m_renderers)
        {
          if (renderer.get() == output->renderer())
          {
            renderer->release();
          }

          // TODO shouldn't that be in that "if" above ? we only resize
          // one viewport at a time...
          renderer.reset();
          renderer = createRenderList(output, *output->renderState());
        }
      };

      // TODO only works for one output !!
      output->createOutput(
          graphicsApi,
          onReady,
          [this] {
            switch (this->m_outputs.size())
            {
              case 1:
                if (this->m_vsync_callback)
                  this->m_vsync_callback();
                break;
              default:
                break;
            }
          },
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

    r.renderedNodes.clear();

    auto& model_nodes = r.nodes;
    {
      // In which order do we want to render stuff
      int processed = 0;
      while (processed != model_nodes.size())
      {
        graphwalk(model_nodes[processed], model_nodes);
        processed++;
      }
      std::reverse(model_nodes.begin(), model_nodes.end());

      if (model_nodes.size() > 1)
      {
        for (auto node : model_nodes)
        {
          auto rn = node->renderedNodes[&r];
          if (!rn)
          {
            rn = node->createRenderer(r);
            node->renderedNodes[&r] = rn;
            rn->init(r);
          }
          else
          {
            rn->releaseWithoutRenderTarget(r);
            rn->init(r);
          }
          SCORE_ASSERT(rn);
          r.renderedNodes.push_back(rn);
        }
      }
      else if (model_nodes.size() == 1)
      {
        auto rn = model_nodes[0]->renderedNodes[&r];
        assert(rn);
        rn->release(r);
      }
    }
    r.output->onRendererChange();
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
  r.renderedNodes.push_back(rn);

  // Register the rendered nodes with their parents
  node.renderedNodes[&r] = rn;
}

std::shared_ptr<RenderList>
Graph::createRenderList(OutputNode* output, RenderState state)
{
  auto ptr = std::make_shared<RenderList>();
  for (auto& node : m_nodes)
    node->addedToGraph = false;

  RenderList& r = *ptr;
  r.output = output;
  output->setRenderer(ptr.get());
  r.state = std::move(state);

  auto& model_nodes = r.nodes;
  {
    model_nodes.push_back(output);

    // In which order do we want to render stuff
    std::size_t processed = 0;
    while (processed != model_nodes.size())
    {
      graphwalk(model_nodes[processed], model_nodes);
      processed++;
    }
    std::reverse(model_nodes.begin(), model_nodes.end());

    // Now we have the nodes in the order in which they are going to
    // be processed
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
      for (auto rn : r.renderedNodes)
        rn->init(r);
    }
  }

  return ptr;
}

Graph::Graph() { }

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

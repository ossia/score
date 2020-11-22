#include "graph.hpp"

#include "nodes.hpp"
#include "renderer.hpp"
#include "window.hpp"

#include <score/tools/Debug.hpp>
#include <QVulkanInstance>
static void graphwalk(score::gfx::Node* node, std::vector<score::gfx::Node*>& list)
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

#if QT_CONFIG(vulkan)
QVulkanInstance* staticVulkanInstance()
{
  static bool vulkanInstanceCreated = false;
  static bool vulkanInstanceInvalid = false;
  if(vulkanInstanceInvalid)
    return nullptr;

  static QVulkanInstance vulkanInstance;
  if(vulkanInstanceCreated)
    return &vulkanInstance;

#if !defined(NDEBUG)
  vulkanInstance.setLayers({"VK_LAYER_KHRONOS_validation"});
#endif
  vulkanInstance.setExtensions(QByteArrayList() << "VK_KHR_get_physical_device_properties2");

  if (!vulkanInstance.create())
  {
    vulkanInstanceInvalid = true;
    return nullptr;
  }
  vulkanInstanceCreated = true;
  return &vulkanInstance;
}
#endif


void Graph::setupOutputs(GraphicsApi graphicsApi)
{
#if QT_CONFIG(vulkan)
  if (graphicsApi == Vulkan)
  {
    if(!staticVulkanInstance())
    {
      qWarning("Failed to create Vulkan instance, switching to OpenGL");
      graphicsApi = OpenGL;
    }
  }
#endif

#if __APPLE__
  graphicsApi = Metal;
#endif

#ifdef Q_OS_WIN
  graphicsApi = D3D11;
#endif

  for (auto output : outputs)
  {
    output->stopRendering();
  }

  for (auto node : nodes)
  {
    node->renderedNodes.clear();
  }

  for (auto& renderer : renderers)
  {
    renderer->release();

    for (auto rn : renderer->renderedNodes)
      delete rn;
  }

  renderers.clear();
  outputs.clear();

  for (auto node : nodes)
    if (auto out = dynamic_cast<OutputNode*>(node))
      outputs.push_back(out);

  renderers.reserve(std::max((int)16, (int)outputs.size()));

  for (auto output : outputs)
  {
    if (!output->canRender())
    {
      auto onReady = [=] {
        renderers.push_back(createRenderer(output, *output->renderState()));
      };
      auto onResize = [=] {
        for (std::shared_ptr<Renderer>& renderer : this->renderers)
        {
          if (renderer.get() == output->renderer())
          {
            renderer->release();
          }

          // TODO shouldn't that be in that "if" above ? we only resize
          // one viewport at a time...
          renderer.reset();
          renderer = createRenderer(output, *output->renderState());
        }
      };

      // TODO only works for one output !!
      output->createOutput(graphicsApi, onReady, vsync_callback, onResize);
    }
    else
    {
      if(auto rs = output->renderState())
      {
        renderers.push_back(createRenderer(output, *rs));
      }
      // output->window->state.hasSwapChain = true;
    }

    output->startRendering();
  }
}

void Graph::relinkGraph()
{
  for (auto& rptr : renderers)
  {
    auto& r = *rptr;
    for (auto& node : nodes)
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
            rn = node->createRenderer();
            if (node != model_nodes.back())
            {
              rn->createRenderTarget(r.state);
            }
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

void Graph::setVSyncCallback(std::function<void ()> cb)
{
  // TODO thread safety if vulkan uses a thread ?
  // If we have more than one output, then instead we sync them with
  // a simple timer, as they may have drastically different vsync rates.
  if(cb)
  {
    vsync_callback = [this, callback = std::move(cb)] {
        switch(this->outputs.size())
        {
          case 1:
            callback();
            break;
          default:
            break;
        }
    };
  }
  else
  {

  }
}

static void createNodeRenderer(score::gfx::Node& node, Renderer& r)
{
  auto rn = node.createRenderer();

  // Register the node with the renderer
  r.renderedNodes.push_back(rn);

  // Register the rendered nodes with their parents
  node.renderedNodes[&r] = rn;
}

std::shared_ptr<Renderer> Graph::createRenderer(OutputNode* output, RenderState state)
{
  auto ptr = std::make_shared<Renderer>();
  for (auto& node : nodes)
    node->addedToGraph = false;

  Renderer& r = *ptr;
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

  // For each, we create a render target
  r.createRenderTargets();

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

Graph::~Graph()
{
  for (auto& renderer : renderers)
  {
    renderer->release();
  }

  for (auto out : outputs)
  {
    out->destroyOutput();
  }
}

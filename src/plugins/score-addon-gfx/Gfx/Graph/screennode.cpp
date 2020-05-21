#include "nodes.hpp"
#include "window.hpp"
#include "graph.hpp"


ScreenNode::ScreenNode()
  : OutputNode{}
{
  input.push_back(new Port{this, {}, Types::Image, {}});
}

bool ScreenNode::canRender() const
{
  return bool(window);
}

void ScreenNode::startRendering()
{
  if (window)
  {
    window->onRender = [this] {
    if (auto r = window->state.renderer)
    {
      window->canRender = r->renderedNodes.size() > 1;
      r->render();
    }
    };
  }
}

void ScreenNode::onRendererChange()
{
  if (window)
    if (auto r = window->state.renderer)
      window->canRender = r->renderedNodes.size() > 1;
}
void ScreenNode::stopRendering()
{
  if (window)
  {
    window->canRender = false;
    window->onRender = [] {};
    ////window->state.hasSwapChain = false;
  }
}

void ScreenNode::setRenderer(Renderer* r)
{
  window->state.renderer = r;
}

Renderer* ScreenNode::renderer() const
{
  if(window)
    return window->state.renderer;
  else
    return nullptr;
}

void ScreenNode::createOutput(GraphicsApi graphicsApi, std::function<void ()> onReady, std::function<void ()> onResize)
{
  window = std::make_shared<Window>(graphicsApi);

#if QT_CONFIG(vulkan)
  if (graphicsApi == Vulkan)
    window->setVulkanInstance(staticVulkanInstance());
#endif
  window->onWindowReady = [this, graphicsApi, onReady] {
    window->state = RenderState::create(*window, graphicsApi);

    onReady();
  };
  window->onResize = onResize;
  window->resize(1280, 720);
  window->show();
}

void ScreenNode::destroyOutput()
{
  window.reset();
}

RenderState* ScreenNode::renderState() const
{
  if(window)
  {
    return &window->state;
  }
  return nullptr;
}

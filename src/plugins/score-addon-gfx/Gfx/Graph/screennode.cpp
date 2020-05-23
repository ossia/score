#include "nodes.hpp"
#include "window.hpp"
#include "graph.hpp"


static RenderState createRenderState(QWindow& window, GraphicsApi graphicsApi)
{
  RenderState state;

  if (graphicsApi == Null)
  {
    QRhiNullInitParams params;
    state.rhi = QRhi::create(QRhi::Null, &params, {});
  }

#ifndef QT_NO_OPENGL
  if (graphicsApi == OpenGL)
  {
    state.surface = QRhiGles2InitParams::newFallbackSurface();
    QRhiGles2InitParams params;
    params.fallbackSurface = state.surface;
    params.window = &window;
    state.rhi = QRhi::create(QRhi::OpenGLES2, &params, {});
  }
#endif

#if QT_CONFIG(vulkan)
  if (graphicsApi == Vulkan)
  {
    QRhiVulkanInitParams params;
    params.inst = window.vulkanInstance();
    params.window = &window;
    state.rhi = QRhi::create(QRhi::Vulkan, &params, {});
  }
#endif

#ifdef Q_OS_WIN
  if (graphicsApi == D3D11)
  {
    QRhiD3D11InitParams params;
    params.enableDebugLayer = false;
    // if (framesUntilTdr > 0)
    // {
    //   params.framesUntilKillingDeviceViaTdr = framesUntilTdr;
    //   params.repeatDeviceKill = true;
    // }
    state.rhi = QRhi::create(QRhi::D3D11, &params, {});
  }
#endif

#ifdef Q_OS_DARWIN
  if (graphicsApi == Metal)
  {
    QRhiMetalInitParams params;
    state.rhi = QRhi::create(QRhi::Metal, &params, {});
    if (!state.rhi)
      qFatal("Failed to create METAL backend");
  }
#endif

  if (!state.rhi)
    qFatal("Failed to create RHI backend");

  state.size = window.size();

  return state;
}


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
    window->onRender = [this] (QRhiCommandBuffer& commands) {
    if (auto r = window->state.renderer)
    {
      window->canRender = r->renderedNodes.size() > 1;
      r->render(commands);
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
    window->onRender = [] (QRhiCommandBuffer&) {};
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
    window->state = createRenderState(*window, graphicsApi);
    {

      swapChain = window->state.rhi->newSwapChain();
      window->swapChain = swapChain;

      // state.renderBuffer = state.rhi->newRenderBuffer(
      //            QRhiRenderBuffer::DepthStencil,
      //            QSize(),
      //            1,
      //            QRhiRenderBuffer::UsedWithSwapChainOnly);
      //
      swapChain->setWindow(window.get());
      // swapChain->setDepthStencil(state.renderBuffer);
      swapChain->setSampleCount(1);
      swapChain->setFlags({});
      window->state.renderPassDescriptor = swapChain->newCompatibleRenderPassDescriptor();
      swapChain->setRenderPassDescriptor(window->state.renderPassDescriptor);
    }

    onReady();
  };
  window->onResize = onResize;
  window->resize(1280, 720);
  window->show();
}

void ScreenNode::destroyOutput()
{
  if(!window)
    return;

  auto& s = window->state;
  delete s.renderPassDescriptor;
  s.renderPassDescriptor = nullptr;

  delete s.renderBuffer;
  s.renderBuffer = nullptr;

  delete swapChain;
  swapChain = nullptr;
  window->swapChain = nullptr;

  delete s.rhi;
  s.rhi = nullptr;

  delete s.surface;
  s.surface = nullptr;

  window.reset();
}

RenderState* ScreenNode::renderState() const
{
  if(window)
    return &window->state;
  return nullptr;
}

class WindowRenderer : public RenderedNode
{
public:
  using RenderedNode::RenderedNode;
  void createRenderTarget(const RenderState& state) override
  {
    auto& self = static_cast<const ScreenNode&>(this->node);
    if(self.swapChain)
    {
      m_renderTarget = self.swapChain->currentFrameRenderTarget();
      m_renderPass = state.renderPassDescriptor;
    }
    else
    {
      m_renderTarget = nullptr;
      m_renderPass = nullptr;
      qDebug() << "Warning: swapchain not found in screenRenderTarget";
    }
  }
};

RenderedNode* ScreenNode::createRenderer() const noexcept
{
  return new WindowRenderer{*this};
}

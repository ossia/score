#include "nodes.hpp"
#include "window.hpp"
#include "graph.hpp"


#include <QtGui/private/qrhinull_p.h>

#ifndef QT_NO_OPENGL
#include <QOffscreenSurface>
#include <QtGui/private/qrhigles2_p.h>
#endif

#if QT_CONFIG(vulkan)
#include <QtGui/private/qrhivulkan_p.h>
#endif

#ifdef Q_OS_WIN
#include <QtGui/private/qrhid3d11_p.h>
#endif

#ifdef Q_OS_DARWIN
#include <QtGui/private/qrhimetal_p.h>
#endif

#include <QOffscreenSurface>
#include <QWindow>


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
    qDebug() << ("Failed to create RHI backend");

  state.size = window.size();

  return state;
}


ScreenNode::ScreenNode(bool embedded, bool fullScreen)
  : OutputNode{}
  , m_embedded{embedded}
  , m_fullScreen{fullScreen}
  , m_ownsWindow{true}
{
  input.push_back(new Port{this, {}, Types::Image, {}});
}

// Used for the EGL full screen case where we just have a single window
// anyways, which must be running before everything (else QVulkanInstance crashes)
ScreenNode::ScreenNode(std::shared_ptr<Window> w)
  : OutputNode{}
  , m_embedded{false}
  , m_ownsWindow{false}
  , window{std::move(w)}
{
  window->showFullScreen();
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
      window->m_canRender = r->renderedNodes.size() > 1;
      r->render(commands);
    }
    };
  }
}

void ScreenNode::onRendererChange()
{
  if (window)
    if (auto r = window->state.renderer)
      window->m_canRender = r->renderedNodes.size() > 1;
}
void ScreenNode::stopRendering()
{
  if (window)
  {
    window->m_canRender = false;
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

void ScreenNode::createOutput(GraphicsApi graphicsApi,
                              std::function<void ()> onReady,
                              std::function<void ()> onUpdate,
                              std::function<void ()> onResize)
{
  if(m_ownsWindow)
    window = std::make_shared<Window>(graphicsApi);

#if QT_CONFIG(vulkan)
  if (graphicsApi == Vulkan)
    window->setVulkanInstance(staticVulkanInstance());
#endif
  window->onUpdate = std::move(onUpdate);
  window->onWindowReady = [this, graphicsApi, onReady=std::move(onReady)] {
    window->state = createRenderState(*window, graphicsApi);
    if(window->state.rhi)
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

      onReady();
    }
  };
  window->onResize = std::move(onResize);

  if(!m_embedded)
  {
    /*
    if(window->isExposed())
    {
      window->exposeEvent(nullptr);
    }
    */

    if(m_fullScreen)
    {
      window->showFullScreen();
    }
    else
    {
      window->resize(1280, 720);
      window->show();
    }
  }
}

void ScreenNode::destroyOutput()
{
  if(!window)
    return;

  auto& s = window->state;
  delete s.renderPassDescriptor;
  s.renderPassDescriptor = nullptr;

  //delete s.renderBuffer;
  //s.renderBuffer = nullptr;

  delete swapChain;
  swapChain = nullptr;
  window->swapChain = nullptr;

  delete s.rhi;
  s.rhi = nullptr;

  delete s.surface;
  s.surface = nullptr;

  if(m_ownsWindow)
    window.reset();
}

RenderState* ScreenNode::renderState() const
{
  if(window && window->swapChain)
    return &window->state;
  return nullptr;
}

class WindowRenderer : public RenderedNode
{
public:
  using RenderedNode::RenderedNode;
  TextureRenderTarget createRenderTarget(const RenderState& state) override
  {
    auto& self = static_cast<const ScreenNode&>(this->node);
    if(self.swapChain)
    {
      m_rt.renderTarget = self.swapChain->currentFrameRenderTarget();
      m_rt.renderPass = state.renderPassDescriptor;
    }
    else
    {
      m_rt.renderTarget = nullptr;
      m_rt.renderPass = nullptr;
      qDebug() << "Warning: swapchain not found in screenRenderTarget";
    }
    return m_rt;
  }
};

score::gfx::NodeRenderer* ScreenNode::createRenderer() const noexcept
{
  return new WindowRenderer{*this};
}

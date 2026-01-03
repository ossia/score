#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/ScreenNode.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/gfx/OpenGL.hpp>

#include <QtGui/private/qrhinull_p.h>

#ifndef QT_NO_OPENGL
#include <Gfx/Settings/Model.hpp>

#include <Gfx/InvertYRenderer.hpp>
#include <QOffscreenSurface>
#include <QtGui/private/qrhigles2_p.h>
#endif

#include <score/gfx/Vulkan.hpp>
#if QT_HAS_VULKAN
#if __has_include(<QtGui/private/qrhivulkan_p.h>)
#include <QtGui/private/qrhivulkan_p.h>
#if __has_include(<vulkan/vulkan_win32.h>)
#include <vulkan/vulkan.h>
#ifdef Q_OS_WIN
#include <vulkan/vulkan_win32.h>
#endif
#endif
#else
#undef QT_HAS_VULKAN
#endif
#endif

#ifdef Q_OS_WIN
#include <QtGui/private/qrhid3d11_p.h>
#endif

#ifdef Q_OS_DARWIN
#include <QtGui/private/qrhimetal_p.h>
#endif

#include <QOffscreenSurface>
#include <QScreen>
#include <QWindow>

namespace score::gfx
{
std::shared_ptr<RenderState>
createRenderState(GraphicsApi graphicsApi, QSize sz, QWindow* window)
{
  auto st = std::make_shared<RenderState>();
  RenderState& state = *st;
  state.api = graphicsApi;

  const auto& settings = score::AppContext().settings<Gfx::Settings::Model>();
  state.samples = settings.resolveSamples(graphicsApi);

  QRhi::Flags flags{};
#ifndef NDEBUG
  flags |= QRhi::EnableDebugMarkers;
#endif

#ifndef QT_NO_OPENGL
  if(graphicsApi == OpenGL)
  {
    state.surface = QRhiGles2InitParams::newFallbackSurface();
    QRhiGles2InitParams params;
    if(window)
    {
      params.format = window->format();
      params.window = window;
    }
    params.fallbackSurface = state.surface;

    score::GLCapabilities caps;
    caps.setupFormat(params.format);
    params.format.setSamples(state.samples);
    state.version = caps.qShaderVersion;
    state.rhi = QRhi::create(QRhi::OpenGLES2, &params, flags);
    state.renderSize = sz;
    return st;
  }
#endif

#if QT_HAS_VULKAN
  if(graphicsApi == Vulkan)
  {
    QRhiVulkanInitParams params;
    params.deviceExtensions = QRhiVulkanInitParams::preferredInstanceExtensions();
#if defined(_WIN32)
    params.deviceExtensions << VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME
                            << VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME
                            << VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME
                            << VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME
                            << VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME
                            << VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME
        ;
#endif

    if(window)
    {
      params.inst = window->vulkanInstance();
      params.window = window;
    }
    else
    {
      params.inst = score::gfx::staticVulkanInstance();
    }
    state.version = Gfx::Settings::shaderVersionForAPI(Vulkan);
    state.rhi = QRhi::create(QRhi::Vulkan, &params, flags);
    state.renderSize = sz;
    return st;
  }
#endif

#ifdef Q_OS_WIN
  if(graphicsApi == D3D11)
  {
    QRhiD3D11InitParams params;
#if !defined(NDEBUG)
    params.enableDebugLayer = true;
#endif
    // if (framesUntilTdr > 0)
    // {
    //   params.framesUntilKillingDeviceViaTdr = framesUntilTdr;
    //   params.repeatDeviceKill = true;
    // }
    state.version = Gfx::Settings::shaderVersionForAPI(D3D11);
    state.rhi = QRhi::create(QRhi::D3D11, &params, flags);
    state.renderSize = sz;
    return st;
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
  else if(graphicsApi == D3D12)
  {
    QRhiD3D12InitParams params;
#if !defined(NDEBUG)
    params.enableDebugLayer = true;
#endif
    // if (framesUntilTdr > 0)
    // {
    //   params.framesUntilKillingDeviceViaTdr = framesUntilTdr;
    //   params.repeatDeviceKill = true;
    // }
    state.version = Gfx::Settings::shaderVersionForAPI(D3D12);
    state.rhi = QRhi::create(QRhi::D3D12, &params, flags);
    state.renderSize = sz;
    return st;
  }
#endif
#endif

#ifdef Q_OS_DARWIN
  if(graphicsApi == Metal)
  {
    QRhiMetalInitParams params;
    state.version = Gfx::Settings::shaderVersionForAPI(Metal);
    state.rhi = QRhi::create(QRhi::Metal, &params, flags);
    state.renderSize = sz;
    return st;
  }
#endif

  if(!state.rhi)
  {
    qDebug() << "Failed to create RHI backend, creating Null backend";

    QRhiNullInitParams params;
    state.version = QShaderVersion(120);
    state.rhi = QRhi::create(QRhi::Null, &params, flags);
    state.renderSize = sz;
    state.api = GraphicsApi::Null;
    return st;
  }

  return st;
}

static std::shared_ptr<RenderState>
createRenderState(QWindow& window, GraphicsApi graphicsApi)
{
  return createRenderState(graphicsApi, window.size(), &window);
}

ScreenNode::ScreenNode(Configuration conf, bool embedded, bool fullScreen)
    : OutputNode{}
    , m_conf{conf}
    , m_embedded{embedded}
    , m_fullScreen{fullScreen}
    , m_ownsWindow{true}
{
  input.push_back(new Port{this, {}, Types::Image, {}});
}

ScreenNode::~ScreenNode()
{
  if(m_swapChain)
  {
    m_swapChain->deleteLater();

    if(m_window)
    {
      m_window->m_swapChain = nullptr;
      m_window->m_hasSwapChain = false;
    }
  }

  if(m_window && m_window->state)
  {
    delete m_window->state->renderPassDescriptor;
    m_window->state->renderPassDescriptor = nullptr;

    delete m_depthStencil;
    m_depthStencil = nullptr;

    delete m_window->state->rhi;
    m_window->state->rhi = nullptr;

    delete m_window->state->surface;
    m_window->state->surface = nullptr;
  }
}

bool ScreenNode::canRender() const
{
  // FIXME - Graph::onReady / Graph::onResize :
  // swapchain is recreated even when size does not change at the
  // beginning
  return bool(m_window) /* && m_window->m_hasSwapChain */;
}

void ScreenNode::startRendering()
{
  if(m_window)
  {
    m_window->onRender = [this](QRhiCommandBuffer& commands) {
      if(auto r = m_window->state->renderer.lock())
      {
        m_window->m_canRender = r->renderers.size() > 1;
        r->render(commands);
      }
    };
  }
}

void ScreenNode::render()
{
  // Used when we don't have vsync: request an update on the Window
  if(m_window)
  {
    onRendererChange();
    m_window->render();
  }
}

void ScreenNode::onRendererChange()
{
  if(m_window)
  {
    if(m_window->state)
    {
      if(auto r = m_window->state->renderer.lock())
      {
        m_window->m_canRender = r->renderers.size() > 1;
        return;
      }
    }
  }
  m_window->m_canRender = false;
}

void ScreenNode::stopRendering()
{
  if(m_window)
  {
    m_window->m_canRender = false;
    m_window->onRender = [](QRhiCommandBuffer&) {};
    if(m_window->state)
      m_window->state->renderer = {};
    ////window->state->hasSwapChain = false;
  }
}

void ScreenNode::setRenderer(std::shared_ptr<RenderList> r)
{
  m_window->state->renderer = r;
}

RenderList* ScreenNode::renderer() const
{
  if(m_window && m_window->state)
    return m_window->state->renderer.lock().get();
  else
    return nullptr;
}

void ScreenNode::setScreen(QScreen* scr)
{
  m_screen = scr;
  if(m_window)
  {
    m_window->setScreen(scr);
  }
}

void ScreenNode::setPosition(QPoint pos)
{
  m_pos = pos;
  if(m_window)
  {
    m_window->setPosition(pos);
  }
}

void ScreenNode::setTitle(QString title)
{
  m_title = title;
  if(m_window)
  {
    m_window->setTitle(title);
  }
}

void ScreenNode::setSize(QSize sz)
{
  m_sz = sz;
  if(m_window)
  {
    m_window->setGeometry({m_window->position(), sz});
  }
}

void ScreenNode::setRenderSize(QSize sz)
{
  if(sz.width() >= 1 && sz.height() >= 1)
    m_renderSz = sz;
  else
    m_renderSz = std::nullopt;

  if(m_window && m_window->onResize)
  {
    m_window->onResize();
  }
}

void ScreenNode::setFullScreen(bool b)
{
  m_fullScreen = b;
  if(m_window)
  {
    if(b)
    {
      m_window->showFullScreen();
    }
    else
    {
      m_window->showNormal();
    }
  }
}

void ScreenNode::setCursor(bool b)
{
  if(m_window)
  {
    if(b && m_window->cursor() == Qt::BlankCursor)
    {
      m_window->unsetCursor();
    }
    else if(!b && m_window->cursor() != Qt::BlankCursor)
    {
      m_window->setCursor(Qt::BlankCursor);
    }
  }
}

void ScreenNode::createOutput(score::gfx::OutputConfiguration conf)
{
  if(m_ownsWindow)
  {
    m_window = std::make_shared<Window>(conf.graphicsApi);
    if(m_embedded)
      m_window->unsetCursor();
  }

  QObject::connect(m_window.get(), &Window::xChanged, [this](int x) {
    if(onWindowMove)
      onWindowMove(QPointF(x, m_window->y()));
  });
  QObject::connect(m_window.get(), &Window::yChanged, [this](int y) {
    if(onWindowMove)
      onWindowMove(QPointF(m_window->x(), y));
  });
  QObject::connect(m_window.get(), &Window::mouseMove, [this](QPointF s, QPointF w) {
    if(onMouseMove)
      onMouseMove(s, w);
  });
  QObject::connect(m_window.get(), &Window::tabletMove, [this](QTabletEvent* e) {
    if(onTabletMove)
      onTabletMove(e);
  });
  QObject::connect(m_window.get(), &Window::key, [this](int k, const QString& t) {
    if(onKey)
      onKey(k, t);
  });
  QObject::connect(m_window.get(), &Window::keyRelease, [this](int k, const QString& t) {
    if(onKeyRelease)
      onKeyRelease(k, t);
  });
  m_window->onUpdate = this->m_vsyncCallback;
  m_window->onWindowReady = [this, graphicsApi=conf.graphicsApi, onReady = std::move(conf.onReady)] {
    m_window->state = createRenderState(*m_window, graphicsApi);
    m_window->state->window = m_window;
    m_window->state->renderSize = QSize(1280, 720);
    if(m_window->state->rhi)
    {
      // TODO depth stencil, render buffer, etc ?
      m_swapChain = m_window->state->rhi->newSwapChain();
      m_swapChain->setName("ScreenNode::m_swapChain");
      m_window->m_swapChain = m_swapChain;
      m_depthStencil = m_window->state->rhi->newRenderBuffer(
          QRhiRenderBuffer::DepthStencil,
          QSize(), // no need to set the size here, due to UsedWithSwapChainOnly
          m_window->state->samples, QRhiRenderBuffer::UsedWithSwapChainOnly);
      m_depthStencil->setName("ScreenNode::m_depthStencil");
      m_swapChain->setWindow(m_window.get());
      m_swapChain->setDepthStencil(m_depthStencil);
      m_swapChain->setSampleCount(m_window->state->samples);

      QRhiSwapChain::Flags flags = QRhiSwapChain::MinimalBufferCount;
      if(!score::AppContext().settings<Gfx::Settings::Model>().getVSync())
        flags |= QRhiSwapChain::NoVSync;
      m_swapChain->setFlags(flags);

      m_window->state->renderPassDescriptor
          = m_swapChain->newCompatibleRenderPassDescriptor();
      m_swapChain->setRenderPassDescriptor(m_window->state->renderPassDescriptor);

      onReady();
    }
  };
  m_window->onResize = [this, onResize = std::move(conf.onResize)] {
    if(m_window && m_window->state)
    {
      auto& st = *m_window->state;
      if(!this->m_renderSz)
      {
        st.renderSize = st.outputSize;
      }
      else
      {
        st.renderSize = *this->m_renderSz;
      }
    }

    if(onResize)
    {
      onResize();
    }
  };

  if(!m_embedded)
  {
    /*
    if(window->isExposed())
    {
      window->exposeEvent(nullptr);
    }
    */

    if(!m_title.isEmpty())
      m_window->setTitle(m_title);

    if(m_screen)
    {
      m_window->setScreen(m_screen);
    }

    if(m_fullScreen)
    {
      m_window->showFullScreen();
    }
    else
    {
      if(m_pos)
      {
        m_window->setPosition(*m_pos);
      }

      if(!m_sz)
      {
        m_window->resize(1280, 720);
      }
      else
      {
        m_window->setGeometry({m_window->position(), *m_sz});
      }
      m_window->show();
    }
  }
}

void ScreenNode::destroyOutput()
{
  if(!m_window)
    return;

  delete m_depthStencil;
  m_depthStencil = nullptr;

  if(auto s = m_window->state)
  {
    delete s->renderPassDescriptor;
    s->renderPassDescriptor = nullptr;
  }

  //delete s.renderBuffer;
  //s.renderBuffer = nullptr;

  delete m_swapChain;
  m_swapChain = nullptr;
  m_window->m_swapChain = nullptr;

  if(auto s = m_window->state)
  {
    s->destroy();
  }

  if(m_ownsWindow)
  {
    m_window.reset();
  }
}

void ScreenNode::updateGraphicsAPI(GraphicsApi api)
{
  if(!m_window)
    return;

  if(m_window->api() != api)
  {
    destroyOutput();
  }
  else if(this->m_window)
  {
    if(this->m_window->state)
    {
      const int samples
          = score::AppContext().settings<Gfx::Settings::Model>().resolveSamples(api);

      if(this->m_window->state->samples != samples)
      {
        destroyOutput();
      }
    }
  }
}

void ScreenNode::setVSyncCallback(std::function<void ()> f)
{
  // TODO thread safety if vulkan uses a thread ?
  // If we have more than one output, then instead we sync them with
  // a simple timer, as they may have drastically different vsync rates.
  m_vsyncCallback = f;
  if(m_window)
    m_window->onUpdate = m_vsyncCallback;
}

std::shared_ptr<score::gfx::RenderState> ScreenNode::renderState() const
{
  if(m_window && m_window->m_swapChain)
    return m_window->state;
  return nullptr;
}

score::gfx::OutputNodeRenderer* ScreenNode::createRenderer(RenderList& r) const noexcept
{
  score::gfx::TextureRenderTarget rt;
  rt.renderTarget = m_swapChain->currentFrameRenderTarget();
  rt.renderPass = r.state.renderPassDescriptor;
  // FIXME why doesn't it work?
  // return new BasicRenderer{rt, r.state, *this};
  return new Gfx::ScaledRenderer{rt, r.state, *this};
}

OutputNode::Configuration ScreenNode::configuration() const noexcept
{
  return m_conf;
}


}

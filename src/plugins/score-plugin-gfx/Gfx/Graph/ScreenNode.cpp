#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/ScreenNode.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/gfx/OpenGL.hpp>
#include <score/tools/Debug.hpp>

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
#include <Gfx/Graph/VulkanVideoDevice.hpp>
#include <Gfx/Graph/interop/VkExternalMemoryHelpers.hpp>
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

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QOffscreenSurface>
#include <QScreen>
#include <QStandardPaths>
#include <QThreadPool>
#include <QWindow>

#include <utility>

namespace score::gfx
{
namespace
{
// Persistent pipeline cache. Saved on QRhi destruction, loaded right after
// QRhi creation. Keyed per backend so different APIs don't overwrite each
// other's cache. Gated on QRhi::Feature::PipelineCacheDataLoadSave.
static QString pipelineCacheFilePath(GraphicsApi api)
{
  QString root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  if(root.isEmpty())
    root = QDir::tempPath();
  QDir().mkpath(root + QStringLiteral("/ossia-score/pipeline-cache"));
  const char* apiName = "unknown";
  switch(api)
  {
    case Null:   apiName = "null"; break;
    case OpenGL: apiName = "gl"; break;
    case Vulkan: apiName = "vk"; break;
    case D3D11:  apiName = "d3d11"; break;
    case D3D12:  apiName = "d3d12"; break;
    case Metal:  apiName = "metal"; break;
  }
  return QStringLiteral("%1/ossia-score/pipeline-cache/%2.bin")
      .arg(root)
      .arg(QString::fromLatin1(apiName));
}

static void tryLoadPipelineCache(QRhi* rhi, GraphicsApi api)
{
  if(!rhi || !rhi->isFeatureSupported(QRhi::PipelineCacheDataLoadSave))
    return;
  QFile f(pipelineCacheFilePath(api));
  if(!f.open(QIODevice::ReadOnly))
    return;
  rhi->setPipelineCacheData(f.readAll());
}

// Pure disk I/O — no QRhi access, so it is safe to run off the render thread.
static void writePipelineCacheToDisk(QByteArray data, GraphicsApi api)
{
  if(data.isEmpty())
    return;
  QFile f(pipelineCacheFilePath(api));
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return;
  f.write(data);
}

// Synchronous store: grabs the cache bytes from the QRhi (must be on the
// render thread) and writes them inline. Used on shutdown (preRhiDestroy)
// where the QRhi is about to be destroyed and we must finish before it goes.
static void tryStorePipelineCache(QRhi* rhi, GraphicsApi api)
{
  if(!rhi || !rhi->isFeatureSupported(QRhi::PipelineCacheDataLoadSave))
    return;
  writePipelineCacheToDisk(rhi->pipelineCacheData(), api);
}

// Mid-session store: grabs the cache bytes on the render thread (QRhi access),
// then offloads the blocking file write to a worker thread so the render
// thread doesn't stall on disk I/O right after a PSO-compile burst. The
// QByteArray is copied into the task (implicitly shared, cheap) and outlives
// the QRhi-independent write.
static void tryStorePipelineCacheAsync(QRhi* rhi, GraphicsApi api)
{
  if(!rhi || !rhi->isFeatureSupported(QRhi::PipelineCacheDataLoadSave))
    return;
  QByteArray data = rhi->pipelineCacheData();
  if(data.isEmpty())
    return;
  QThreadPool::globalInstance()->start(
      [data = std::move(data), api]() mutable {
    writePipelineCacheToDisk(std::move(data), api);
  });
}
}

std::shared_ptr<RenderState>
createRenderState(GraphicsApi graphicsApi, QSize sz, QWindow* window)
{
  auto st = std::make_shared<RenderState>();
  RenderState& state = *st;
  state.api = graphicsApi;

  const auto& settings = score::AppContext().settings<Gfx::Settings::Model>();
  state.samples = settings.resolveSamples(graphicsApi);

  auto populateCaps = [graphicsApi](RenderState& s) {
    // Load persisted pipeline cache (if any) and set up a save-on-destroy
    // hook that writes it back before QRhi is deleted.
    if(s.rhi)
    {
      tryLoadPipelineCache(s.rhi, graphicsApi);
      QRhi* rhiPtr = s.rhi;
      s.preRhiDestroy = [rhiPtr, graphicsApi]() {
        tryStorePipelineCache(rhiPtr, graphicsApi);
      };
      // Plan 09 S6: mid-session flush for crash-resilient cache
      // persistence. RenderList::render throttles this after PSO
      // stalls; the QRhi read happens here on the render thread but the
      // blocking file write is offloaded to a worker so the render
      // thread isn't stalled on disk right after a PSO-compile burst.
      s.savePipelineCache = [rhiPtr, graphicsApi]() {
        tryStorePipelineCacheAsync(rhiPtr, graphicsApi);
      };
    }
    if(s.rhi)
    {
      s.caps.populate(*s.rhi);
    }
    // Clamp the requested sample count against what the hardware actually
    // supports. Without this, asking for e.g. 16x MSAA on a card that only
    // does 8x silently mismatches between the value stored in
    // RenderList::m_samples (16) and what QRhi actually uses on render
    // targets (8 — clamped via effectiveSampleCount), which breaks
    // pipeline/RT sample-count matching on Vulkan.
    if(s.rhi && s.samples > 1)
    {
      const auto supported = s.rhi->supportedSampleCounts();
      if(supported.isEmpty())
      {
        s.samples = 1;
      }
      else
      {
        // supportedSampleCounts() is sorted ascending. Pick exact match or
        // the largest value <= requested; otherwise fall back to smallest.
        int chosen = supported.first();
        for(int v : supported)
        {
          if(v == s.samples)
          {
            chosen = v;
            break;
          }
          if(v < s.samples)
            chosen = v;
        }
        if(chosen != s.samples)
        {
          qWarning() << "createRenderState: requested samples=" << s.samples
                     << "not in supported list" << supported
                     << "— clamping to" << chosen;
          s.samples = chosen;
        }
      }
    }
  };

  QRhi::Flags flags{};
#ifndef NDEBUG
  flags |= QRhi::EnableDebugMarkers;
#endif
  // Let the RHI save per-backend pipeline binary cache so subsequent runs
  // skip the initial pipeline compilation cost (big win for Vulkan/D3D12).
  flags |= QRhi::EnablePipelineCacheDataSave;

  // Enable per-command-buffer GPU timestamps. Required for the per-pass
  // GPU timing panel (Plan 09 S6) — without this flag,
  // QRhiCommandBuffer::lastCompletedGpuTime() returns 0 on Vulkan/D3D12/Metal.
  // Negligible overhead when no timer instance is active.
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  flags |= QRhi::EnableTimestamps;
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
    populateCaps(state);
    return st;
  }
#endif

#if QT_HAS_VULKAN
  if(graphicsApi == Vulkan)
  {
    QRhiVulkanInitParams params;
    // External-memory/-semaphore extensions for GPU interop (CUDA P2P, Spout,
    // DMA-BUF). These are required so vkGetMemoryFdKHR / vkGetMemoryWin32HandleKHR
    // resolve — without them the zero-copy capture/output paths (e.g. AJA Vulkan
    // tier-3) can't export a VkBuffer/VkImage to CUDA. The shared-device path
    // (Qt>=6.6) already requests them via sharedVulkanDeviceExtensions(); this
    // covers the fallback QRhi-owned device too. On desktop Linux/Windows these
    // are universally supported; QRhi/vkCreateDevice would fail if not, so they
    // stay platform-gated to where the handle types exist.
#if defined(_WIN32)
    params.deviceExtensions << VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME
                            << VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME
                            << VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME
                            << VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME
                            << VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME
                            << VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME
        ;
#else
    params.deviceExtensions << VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME
                            << VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME
                            << VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME
                            << VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME
#ifdef VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME
                            << VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME
#endif
#ifdef VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME
                            << VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME
#endif
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

    // Create shared VkDevice with video decode queues BEFORE QRhi.
    // Use the first physical device from QVulkanInstance — this matches
    // what QRhi would pick by default.
#if defined(VK_KHR_video_decode_queue) && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    {
      auto sharedDev = createSharedVulkanDevice(params.inst);
      if(sharedDev)
      {
        // The shared device enables every queried feature, so interop fast
        // paths (timeline-semaphore ordering) are legal on it — unlike on
        // QRhi-created devices.
        vkinterop::setDeviceTimelineSemaphoresEnabled(
            sharedDev.timelineSemaphores);
        QRhiVulkanNativeHandles importedHandles;
        importedHandles.physDev = sharedDev.physDev;
        importedHandles.dev = sharedDev.dev;
        importedHandles.gfxQueueFamilyIdx = sharedDev.gfxQueueFamilyIdx;
        importedHandles.gfxQueueIdx = 0;
        importedHandles.gfxQueue = sharedDev.gfxQueue;
        importedHandles.inst = params.inst;

        state.rhi = QRhi::create(QRhi::Vulkan, &params, flags, &importedHandles);
        if(state.rhi)
        {
          state.customDeviceCleanup = [dev = sharedDev.dev, inst = params.inst]() {
            if(auto fn = reinterpret_cast<PFN_vkDestroyDevice>(
                   inst->getInstanceProcAddr("vkDestroyDevice")))
              fn(dev, nullptr);
          };
          state.renderSize = sz;
          populateCaps(state);
          return st;
        }
        sharedDev.destroy();
      }
    }
#endif

    // Fallback: let QRhi create its own VkDevice (no video decode queues)
    if(!state.rhi)
      state.rhi = QRhi::create(QRhi::Vulkan, &params, flags);

    state.renderSize = sz;
    populateCaps(state);
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
    populateCaps(state);
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
    populateCaps(state);
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
    populateCaps(state);
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
    populateCaps(state);
    return st;
  }

  populateCaps(state);
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
    // Order matters: clear the alias + flag on the Window BEFORE releasing
    // the QRhiSwapChain. A queued QExposeEvent landing between the deferred
    // delete and the nullings would otherwise observe the inconsistent
    // state (m_hasSwapChain == true && m_swapChain still aliasing freed
    // memory). See diagnostic 047.
    if(m_window)
    {
      m_window->m_hasSwapChain = false;
      m_window->m_swapChain = nullptr;
    }

    m_swapChain->deleteLater();
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
  if(onFps)
    onFps(0.f);
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
    m_window->m_canRender = false;
  }
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
  if(onFps)
    onFps(0.f);
}

void ScreenNode::setRenderer(std::shared_ptr<RenderList> r)
{
  // m_window can be null after destroyOutput() (which calls m_window.reset()).
  // Reachable from Graph::createOutputRenderList paths after a graphics-API
  // switch / sample-count change / output-disable cycle. Sibling guards
  // already exist in stopRendering and onRendererChange below; this one
  // was missed when those were patched.
  if(m_window && m_window->state)
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

void ScreenNode::setConfiguration(Configuration conf)
{
  m_conf = conf;
}

void ScreenNode::setSwapchainFlag(Gfx::SwapchainFlag flag)
{
  if(m_swapchainFlag == flag)
    return;
  m_swapchainFlag = flag;
  // Live flag change (sRGB toggle) requires the swapchain to be recreated
  // with the new flag bits — setFlags happens in createOutput at line ~667.
  // destroyOutput tears down; Graph::createOutputRenderList rebuilds on
  // next reconcile (same pattern updateGraphicsAPI uses for sample-count).
  if(m_window)
    destroyOutput();
}

void ScreenNode::setSwapchainFormat(Gfx::SwapchainFormat format)
{
  if(m_swapchainFormat == format)
    return;
  m_swapchainFormat = format;
  // Same rebuild rationale as setSwapchainFlag above. setFormat happens at
  // line ~650 inside createOutput; without the rebuild the field stayed
  // updated but the live swapchain kept its prior format (HDR↔SDR toggle
  // was silently inert).
  if(m_window)
    destroyOutput();
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
  QObject::connect(m_window.get(), &Window::fps, [this](float f) {
    if(onFps)
      onFps(f);
  });
  m_window->onUpdate = this->m_vsyncCallback;
  m_window->onWindowReady = [this, graphicsApi=conf.graphicsApi, onReady = std::move(conf.onReady)] {
    m_window->state = createRenderState(*m_window, graphicsApi);
    m_window->state->window = m_window;
    m_window->state->renderSize = QSize(1280, 720);
    m_window->state->renderFormat = (m_swapchainFormat != Gfx::SwapchainFormat::SDR)
        ? QRhiTexture::RGBA32F : QRhiTexture::RGBA8;
    if(m_window->state->rhi)
    {
      // TODO depth stencil, render buffer, etc ?
      m_swapChain = m_window->state->rhi->newSwapChain();
      m_swapChain->setName("ScreenNode::m_swapChain");
#if QT_VERSION > QT_VERSION_CHECK(6,4,0)
      m_swapChain->setFormat((QRhiSwapChain::Format)m_swapchainFormat);
#endif
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
      if(m_swapchainFlag == Gfx::SwapchainFlag::sRGB)
        flags |= QRhiSwapChain::sRGB;
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

  // Drain the GPU before tearing anything down. Without this, queued frames
  // can still reference the swapchain / RPD / depth-stencil while we're
  // freeing them — and worse, when setSwapchainFormat / setSwapchainFlag
  // call destroyOutput synchronously (commit e2afe7874), the host window's
  // last beginFrame may still hold an unfinished cbWrapper referenced by
  // ScenePreprocessor's per-frame copyBuffer (commit fe146c8de). The next
  // runInitialPasses then records vkCmdCopyBuffer / vkCmdPipelineBarrier
  // into a CB whose underlying VkCommandBuffer was already vkEndCommandBuffer'd
  // (VUID-vkCmdCopyBuffer-commandBuffer-recording / VUID-vkCmdPipelineBarrier-
  // commandBuffer-recording), often followed by a device loss.
  //
  // MultiWindowNode::destroyOutput already does this at line ~1068; mirror it.
  if(m_window->state && m_window->state->rhi)
  {
    // Pre-condition: destroyOutput must not be called inside a frame
    // (between beginFrame and endFrame). If this fires, some upstream
    // path triggered a teardown mid-render — the cascade would be
    // worse than just deferring to next frame.
    SCORE_ASSERT(!m_window->state->rhi->isRecordingFrame());
    m_window->state->rhi->finish();
  }

  // Persist-across-rebuild contract: the registry survives RL teardown
  // so we must explicitly release its QRhi resources here, BEFORE
  // RenderState::destroy() (called below via m_window->state->destroy())
  // frees the device. destroyOwned() `delete`s the buffer / texture /
  // sampler wrappers directly while the QRhi is still alive.
  releaseRegistry();

  delete m_depthStencil;
  m_depthStencil = nullptr;

  if(m_window)
  {
    if(auto s = m_window->state)
    {
      delete s->renderPassDescriptor;
      s->renderPassDescriptor = nullptr;
    }
  }

  //delete s.renderBuffer;
  //s.renderBuffer = nullptr;

  // Order matters: clear the alias + flag on the Window BEFORE deleting
  // the QRhiSwapChain (see diagnostic 047). A queued event reaching
  // Window::exposeEvent between the delete and the nulling would
  // otherwise observe (m_hasSwapChain == true && m_swapChain dangling).
  if(m_window)
  {
    m_window->m_hasSwapChain = false;
    m_window->m_swapChain = nullptr;
  }

  delete m_swapChain;
  m_swapChain = nullptr;

  if(m_window)
  {
    if(auto s = m_window->state)
    {
      s->destroy();
    }
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
    if(auto& s = this->m_window->state)
    {
      // FIXME refactor with createRenderState
      // FIXME implement for other output nodes
      int samples_request
          = score::AppContext().settings<Gfx::Settings::Model>().resolveSamples(api);

      if(!s->rhi)
        return;
      const auto supported = s->rhi->supportedSampleCounts();
      if(supported.isEmpty())
      {
        samples_request = 1;
      }
      else
      {
        int chosen = supported.first();
        for(int v : supported)
        {
          if(v == samples_request)
          {
            chosen = v;
            break;
          }
          if(v < samples_request)
            chosen = v;
        }
        if(chosen != samples_request)
        {
          qWarning() << "updateGraphicsAPI: requested samples=" << samples_request
                     << "not in supported list" << supported << "— clamping to"
                     << chosen;
          samples_request = chosen;
        }
      }

      if(this->m_window->state->samples != samples_request)
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
  // No depth attachment exposed here on purpose: ScaledRenderer is a
  // fullscreen-quad blit that samples the upstream color texture and does
  // not run depth test. All precision-critical 3D rendering happens
  // upstream into an intermediate D32F offscreen render target allocated
  // by createRenderTarget(...) in Utils.cpp. The swap chain's D24S8
  // DepthStencil buffer is only attached at the QRhi level for the final
  // blit pass — irrelevant to 3D depth precision.
  // FIXME why doesn't it work?
  // return new BasicRenderer{rt, r.state, *this};
  return new Gfx::ScaledRenderer{rt, r.state, *this};
}

OutputNode::Configuration ScreenNode::configuration() const noexcept
{
  return m_conf;
}


}

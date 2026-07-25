#include <Gfx/Graph/Window.hpp>
#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/gfx/Vulkan.hpp>

#include <core/application/ApplicationInterface.hpp>

#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QTimer>
#include <QtGui/private/qrhigles2_p.h>
#if QT_HAS_VULKAN
#if __has_include(<QtGui/private/qrhivulkan_p.h>)
#include <QtGui/private/qrhivulkan_p.h>
#else
#undef QT_HAS_VULKAN
#endif
#endif
#include <wobjectimpl.h>

W_OBJECT_IMPL(score::gfx::Window)
namespace score::gfx
{

Window::Window(GraphicsApi graphicsApi)
    : m_api{graphicsApi}
{
  setCursor(Qt::BlankCursor);

#if defined(__EMSCRIPTEN__)
  // No OS window manager on wasm: without this the output window drops behind
  // the main window when it's activated and can't be brought back.
  setFlag(Qt::WindowStaysOnTopHint, true);
#endif

  QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();

  // Tell the platform plugin what we want.
  switch(m_api)
  {
    default:
    case OpenGL:
#if QT_CONFIG(opengl)
      setSurfaceType(OpenGLSurface);
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
      fmt = QRhiGles2InitParams::adjustedFormat();
#endif
#endif
    break;

#if QT_HAS_VULKAN
    case Vulkan:
      setSurfaceType(VulkanSurface);
      setVulkanInstance(score::gfx::staticVulkanInstance());
      break;
#endif

#if defined(_WIN32)
    case D3D11:
    case D3D12:
      setSurfaceType(Direct3DSurface);
      break;
#endif

#if defined(__APPLE__)
    case Metal:
      setSurfaceType(MetalSurface);
      break;
#endif
  }

  const auto& settings = score::AppContext().settings<Gfx::Settings::Model>();
  fmt.setSwapInterval(settings.getVSync() ? 1 : 0);

  switch(settings.getBuffers())
  {
    default:
    case 1:
      fmt.setSwapBehavior(QSurfaceFormat::SwapBehavior::SingleBuffer);
      break;
    case 2:
      fmt.setSwapBehavior(QSurfaceFormat::SwapBehavior::DoubleBuffer);
      break;
    case 3:
      fmt.setSwapBehavior(QSurfaceFormat::SwapBehavior::TripleBuffer);
      break;
  }

  const int samples = settings.resolveSamples(m_api);
  fmt.setSamples(samples);

  setFormat(fmt);

  if(auto platform = qGuiApp->platformName();
     platform.contains("eglfs") || platform.contains("vkkhr"))
    m_embeddedFullscreen = true;
}

Window::~Window()
{
  m_closed = true;
}

void Window::init()
{
  onWindowReady();
}

void Window::resizeSwapChain()
{
  if(m_swapChain)
  {
    const QSize surface = m_swapChain->surfacePixelSize();

    // QGles2SwapChain::createOrResize() returns true unconditionally, even for
    // an empty surface: it is not a usable "the swapchain is ready" signal.
    // Building the render list against an empty swapchain gives every node a
    // 1x1 render target (QRhi clamps empty texture sizes up) and a 0x0
    // viewport in the final blit, i.e. a permanently black window, since
    // nothing rebuilds the render list unless the surface size changes again.
    if(surface.isEmpty())
    {
      if(outputLogEnabled())
        qDebug() << "[gfxout] resizeSwapChain: refused, empty surface"
                 << "current=" << m_swapChain->currentPixelSize();
      m_hasSwapChain = false;
      m_newlyExposed = true;
      requestUpdate();
      return;
    }

    m_hasSwapChain = m_swapChain->createOrResize();
    if(state)
      state->outputSize = m_swapChain->currentPixelSize();

    if(outputLogEnabled())
      qDebug() << "[gfxout] resizeSwapChain: ok=" << m_hasSwapChain
               << "surface=" << surface << "current=" << m_swapChain->currentPixelSize()
               << "outputSize=" << (state ? state->outputSize : QSize{})
               << "rt=" << (void*)m_swapChain->currentFrameRenderTarget();

    if(onResize)
      onResize();
  }
  else
  {
    m_hasSwapChain = false;
  }
}

void Window::releaseSwapChain()
{
  if(m_swapChain && m_hasSwapChain)
  {
    if(outputLogEnabled())
      qDebug() << "[gfxout] releaseSwapChain";
    m_hasSwapChain = false;
    m_swapChain->destroy();

    // The render list is built against this swapchain: force a full rebuild
    // when the window comes back rather than reusing it as-is.
    m_newlyExposed = true;
  }
}

void Window::render()
{
  static constexpr double fps_smoothing = .8;
  if(m_closed)
    return;

  if(onUpdate)
  {
    onUpdate();
  }

  if(!m_swapChain)
    return;

  if(!m_hasSwapChain || m_notExposed)
  {
    // wasm delivers a one-shot expose (QWasmWindow::setVisible), so if the
    // surface had no size when exposeEvent latched m_notExposed, nothing ever
    // clears it again and the window stays black. Recover once it has a size.
    if(isExposed() && m_swapChain && !m_swapChain->surfacePixelSize().isEmpty())
    {
      if(outputLogEnabled())
        qDebug() << "[gfxout] render: recovering, hasSwapChain=" << m_hasSwapChain
                 << "notExposed=" << m_notExposed
                 << "surface=" << m_swapChain->surfacePixelSize();
      m_notExposed = false;
      m_newlyExposed = true;
      // fall through: the resize block below will (re)create the swapchain
    }
    else
    {
      requestUpdate();
      return;
    }
  }

  if(m_swapChain->currentPixelSize() != m_swapChain->surfacePixelSize()
     || m_newlyExposed)
  {
    resizeSwapChain();
    if(!m_hasSwapChain)
    {
      requestUpdate();
      return;
    }
    m_newlyExposed = false;
  }

  if(m_canRender && state)
  {
    QRhi::FrameOpResult r = state->rhi->beginFrame(m_swapChain, {});
    if(r == QRhi::FrameOpSwapChainOutOfDate)
    {
      resizeSwapChain();
      if(!m_hasSwapChain)
      {
        requestUpdate();
        return;
      }
      r = state->rhi->beginFrame(m_swapChain);
    }
    if(r != QRhi::FrameOpSuccess)
    {
      requestUpdate();
      return;
    }

    const auto commands = m_swapChain->currentFrameCommandBuffer();
    onRender(*commands);

    state->rhi->endFrame(m_swapChain, {});
    {
      // 1. Calculate the time elapsed since the last frame
      if(const auto frame_ns = m_timer.nsecsElapsed(); frame_ns > 0)
      {
        const double fps = 1e9 / frame_ns;

        // 2. Smooth things a bit
        if(m_fps == 0.0f)
          m_fps = fps;
        else
          m_fps = (fps * fps_smoothing) + (m_fps * (1.0f - fps_smoothing));
      }
      m_timer.restart();
    }
  }
  else
  {
    QRhi::FrameOpResult r = state->rhi->beginFrame(m_swapChain, {});
    if(r == QRhi::FrameOpSwapChainOutOfDate)
    {
      resizeSwapChain();
      if(!m_hasSwapChain)
      {
        requestUpdate();
        return;
      }
      r = state->rhi->beginFrame(m_swapChain);
    }
    if(r != QRhi::FrameOpSuccess)
    {
      requestUpdate();
      return;
    }

    auto buf = m_swapChain->currentFrameCommandBuffer();
    auto batch = state->rhi->nextResourceUpdateBatch();
    buf->beginPass(m_swapChain->currentFrameRenderTarget(), Qt::black, {1.0f, 0}, batch);
    buf->endPass();

    state->rhi->endFrame(m_swapChain, {});
    m_fps = 0.;
  }

  if(m_fpsPushTimer.elapsed() > 50)
  {
    fps(m_fps);
    m_fpsPushTimer.restart();
  }

  if(this->onUpdate) {
    // requestUpdate is only to be used in the vsync case
    requestUpdate();
  }
}

void Window::exposeEvent(QExposeEvent* ev)
{
  if(!onWindowReady)
  {
    return;
  }
  if(outputLogEnabled())
    qDebug() << "[gfxout] exposeEvent: exposed=" << isExposed() << "running=" << m_running
             << "notExposed=" << m_notExposed << "hasSwapChain=" << m_hasSwapChain
             << "size=" << size()
             << "surface=" << (m_swapChain ? m_swapChain->surfacePixelSize() : QSize{});

  if(isExposed() && !m_running)
  {
    m_running = true;
    init();
    resizeSwapChain();
  }

  if(m_hasSwapChain && !m_swapChain)
  {
    qDebug("exposeEvent: m_hasSwapChain && !m_swapChain");
    m_hasSwapChain = false;
  }

  const QSize surfaceSize = m_hasSwapChain ? m_swapChain->surfacePixelSize() : QSize();

  if((!isExposed() || (m_hasSwapChain && surfaceSize.isEmpty())) && m_running)
    m_notExposed = true;

  if(isExposed() && m_running && m_notExposed && !surfaceSize.isEmpty())
  {
    m_notExposed = false;
    m_newlyExposed = true;
  }

  if(isExposed())
  {
    m_closed = false;
  }

  if(isExposed() && !surfaceSize.isEmpty())
  {
    m_timer.restart();
    m_fpsPushTimer.restart();
    render();
  }
}

void Window::mouseDoubleClickEvent(QMouseEvent* ev)
{
  setWindowStates(windowStates() ^ Qt::WindowFullScreen);
}

bool Window::event(QEvent* e)
{
  switch(e->type())
  {
    case QEvent::UpdateRequest:
      render();
      break;

    case QEvent::TabletMove: {
      auto ev = static_cast<QTabletEvent*>(e);
      this->tabletMove(ev);
      this->interactiveEvent(e);
      break;
    }
    case QEvent::TabletPress:
    case QEvent::TabletRelease:
      this->interactiveEvent(e);
      break;

    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
      this->interactiveEvent(e);
      break;

    case QEvent::MouseMove: {
      auto ev = static_cast<QMouseEvent*>(e);
      this->mouseMove(ev->globalPosition(), ev->scenePosition());
      this->interactiveEvent(e);
      break;
    }
    case QEvent::KeyPress: {
      auto ev = static_cast<QKeyEvent*>(e);
      if(!ev->isAutoRepeat())
      {
        this->key(ev->key(), ev->text());
        this->interactiveEvent(e);
        if(ev->key() == Qt::Key_Escape)
          if(m_embeddedFullscreen)
            QMetaObject::invokeMethod(
                qGuiApp, [] { score::GUIApplicationInterface::instance().forceExit(); });
      }

      break;
    }
    case QEvent::KeyRelease: {
      auto ev = static_cast<QKeyEvent*>(e);
      if(!ev->isAutoRepeat())
      {
        this->keyRelease(ev->key(), ev->text());
        this->interactiveEvent(e);
      }
      break;
    }
    case QEvent::PlatformSurface:
      if(static_cast<QPlatformSurfaceEvent*>(e)->surfaceEventType()
         == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) // fallthrough
      case QEvent::Close: {
        releaseSwapChain();
        m_running = false;
        m_hasSwapChain = false;
        m_notExposed = true;
        m_closed = true;
        if(onClose)
          onClose();
#if defined(__EMSCRIPTEN__)
        score::reclaimMainWindowFocus();
#endif
      }
      break;

#if defined(__EMSCRIPTEN__)
    case QEvent::Hide:
      score::reclaimMainWindowFocus();
      break;
#endif

      default:
        break;
  }

  return QWindow::event(e);
}

}

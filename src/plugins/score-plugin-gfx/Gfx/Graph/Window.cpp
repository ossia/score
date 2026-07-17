#include <Gfx/Graph/Window.hpp>
#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/gfx/Vulkan.hpp>

#include <core/application/ApplicationInterface.hpp>

#include <QGuiApplication>
#include <QPointer>
#include <QStringList>

#include <algorithm>

#if defined(__EMSCRIPTEN__)
#include <emscripten/em_asm.h>
#include <emscripten/emscripten.h>
#include <emscripten/val.h>

#include <string>
#endif
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
  // See the note in the JS plugin: without this the output opens full-screen at
  // the viewport size, with no frame to close or move it by.
  setFlag(Qt::Dialog, true);

  // QWasmWindowTreeNode::onSubtreeChanged() activates every window inserted in
  // the tree unless it is a tooltip, a popup, or carries this property.
  setProperty("_q_showWithoutActivating", true);
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
      m_hasSwapChain = false;
      m_newlyExposed = true;
      scheduleRetry();
      return;
    }

    m_hasSwapChain = m_swapChain->createOrResize();
    if(state)
      state->outputSize = m_swapChain->currentPixelSize();


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
    m_hasSwapChain = false;
    m_swapChain->destroy();

    // The render list is built against this swapchain: force a full rebuild
    // when the window comes back rather than reusing it as-is.
    m_newlyExposed = true;
  }
}


void Window::scheduleRetry()
{
  // Retry, but not through requestUpdate(). These paths are the ones taken when
  // the window is not ready or the frame failed, and their condition can be
  // permanent (never exposed, zero-sized surface, a swapchain that will not
  // resize): re-arming a frame callback from inside a frame callback then burns
  // a 60Hz rAF forever and, because each rAF callback's stack is chained to the
  // one that scheduled it, builds an unbounded async stack. Any warning logged
  // from such a loop then carries the whole chain, which is what made DevTools
  // unusable while diagnosing exactly these paths. A timer starts a fresh stack
  // and retries ten times a second, which is plenty for "wait until the canvas
  // has a size".
  if(m_retryScheduled)
    return;

  m_retryScheduled = true;
  QTimer::singleShot(retry_interval_ms, this, [this] {
    m_retryScheduled = false;
    render();
  });
}

void Window::handleDeviceLost()
{
  if(m_deviceLost)
    return;

  m_deviceLost = true;
  m_hasSwapChain = false;
  m_canRender = false;

  // "QRhiGles2: Context is lost." is also emitted, benignly, whenever a QRhi is
  // destroyed -- ScreenNode::destroyOutput() -> RenderState::destroy() ->
  // ~QRhi() -> QRhiGles2::destroy() -> ensureContext(). That path never reaches
  // here (destroyOutput() clears m_swapChain first, so render() returns before
  // beginFrame), and neither does application shutdown, but say nothing on the
  // way out regardless: only a context that dies under a *live* window is a
  // defect worth reporting.
  if(m_closed || QCoreApplication::closingDown())
    return;

  qCritical() << "score::gfx::Window: the graphics context was lost while the output "
                 "was live. This output has stopped rendering.";

  if(onDeviceLost)
  {
    // Deferred, and not through this window: the handler is expected to
    // destroy and rebuild the output, i.e. to delete this.
    QPointer<Window> self{this};
    QMetaObject::invokeMethod(
        qApp,
        [self, cb = onDeviceLost] {
      if(self)
        cb();
    },
        Qt::QueuedConnection);
  }
}

bool Window::checkDeviceLost(int frameOpResult)
{
  if(frameOpResult != QRhi::FrameOpDeviceLost
     && !(state && state->rhi && state->rhi->isDeviceLost()))
    return false;

  handleDeviceLost();
  return true;
}

void Window::render()
{
  static constexpr double fps_smoothing = .8;
  if(m_closed)
    return;

  // A lost context never comes back on its own: without this the window would
  // call beginFrame() on a dead QRhi on every update request and log
  // "QRhiGles2: Context is lost." forever.
  if(m_deviceLost)
    return;

  // Hold a copy across the call: onUpdate() runs updateGraph(), and a graph
  // rebuild triggered from within it can re-arm or clear the vsync callback
  // (clock teardown in recomputeTimers) — destroying the std::function we are
  // currently executing. Copying keeps the callable alive for the duration.
  if(auto f = onUpdate)
  {
    f();
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
      m_notExposed = false;
      m_newlyExposed = true;
      // fall through: the resize block below will (re)create the swapchain
    }
    else
    {
      scheduleRetry();
      return;
    }
  }

  if(m_swapChain->currentPixelSize() != m_swapChain->surfacePixelSize()
     || m_newlyExposed)
  {
    resizeSwapChain();
    if(!m_hasSwapChain)
    {
      scheduleRetry();
      return;
    }
    m_newlyExposed = false;
  }

  if(m_canRender && state)
  {
    QRhi::FrameOpResult r = state->rhi->beginFrame(m_swapChain, {});
    if(checkDeviceLost(r))
      return;
    if(r == QRhi::FrameOpSwapChainOutOfDate)
    {
      resizeSwapChain();
      if(!m_hasSwapChain)
      {
        scheduleRetry();
        return;
      }
      r = state->rhi->beginFrame(m_swapChain);
      if(checkDeviceLost(r))
        return;
    }
    if(r != QRhi::FrameOpSuccess)
    {
      scheduleRetry();
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
    if(checkDeviceLost(r))
      return;
    if(r == QRhi::FrameOpSwapChainOutOfDate)
    {
      resizeSwapChain();
      if(!m_hasSwapChain)
      {
        scheduleRetry();
        return;
      }
      r = state->rhi->beginFrame(m_swapChain);
      if(checkDeviceLost(r))
        return;
    }
    if(r != QRhi::FrameOpSuccess)
    {
      scheduleRetry();
      return;
    }

    auto buf = m_swapChain->currentFrameCommandBuffer();
    auto batch = state->rhi->nextResourceUpdateBatch();
    buf->beginPass(m_swapChain->currentFrameRenderTarget(), Qt::black, {0.0f, 0}, batch);
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

  if(isExposed() && !m_running)
  {
    m_running = true;
    init();
    resizeSwapChain();
  }

  // The teardown sites (ScreenNode / MultiWindowNode destroyOutput) clear
  // the flag before nulling the alias, but they run on the render thread
  // while this runs on the GUI thread with no synchronization — the two
  // plain writes are not ordered for us, so the inconsistent pair IS
  // observable mid-teardown. Self-heal instead of dereferencing null.
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
    case QEvent::DeferredDelete:
      // This Window is owned by a std::shared_ptr (ScreenNode::m_window /
      // Window::state), never by the QObject tree. Honouring a DeferredDelete
      // here runs `delete this` on the interior pointer of a make_shared block
      // (invalid free) and, by destroying Window::state, drops the shared
      // RenderState to RenderList-only ownership so the next
      // Graph::createAllRenderLists frees it out from under the in-flight
      // rebuild (the mid-play use-after-free). A stray deleteLater() on a
      // shared_ptr-managed window is always a bug — swallow it; the shared_ptr
      // deleter will destroy the window at the right time.
      return true;

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


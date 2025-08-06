#include <Gfx/Graph/Window.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/application/ApplicationContext.hpp>
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
  }
}

void Window::render()
{
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
    requestUpdate();
    return;
  }

  if(m_swapChain->currentPixelSize() != m_swapChain->surfacePixelSize()
     || m_newlyExposed)
  {
    resizeSwapChain();
    if(!m_hasSwapChain)
      return;
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
  }
  requestUpdate();
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
    render();
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
      }
      break;

      default:
        break;
  }

  return QWindow::event(e);
}

}

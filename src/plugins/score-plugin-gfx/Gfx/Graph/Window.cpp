#include <Gfx/Graph/Window.hpp>

#include <QPlatformSurfaceEvent>
#include <QTimer>
#include <QtGui/private/qrhigles2_p.h>
namespace score::gfx
{

Window::Window(GraphicsApi graphicsApi)
    : m_api{graphicsApi}
{
  // Tell the platform plugin what we want.
  switch (m_api)
  {
    case OpenGL:
#if QT_CONFIG(opengl)
      setSurfaceType(OpenGLSurface);
      setFormat(QRhiGles2InitParams::adjustedFormat());
#endif
      break;
    case Vulkan:
      setSurfaceType(VulkanSurface);
      break;
    case D3D11:
      setSurfaceType(OpenGLSurface); // not a typo
      break;
    case Metal:
      setSurfaceType(MetalSurface);
      break;
    default:
      break;
  }
}

Window::~Window() { }

void Window::init()
{
  onWindowReady();
}

#include <Gfx/Qt5CompatPush> // clang-format: keep
void Window::resizeSwapChain()
{
  if (m_swapChain)
  {
    m_hasSwapChain = m_swapChain->createOrResize();
    state.size = m_swapChain->currentPixelSize();
    if (onResize)
      onResize();
  }
}

void Window::releaseSwapChain()
{
  if (m_swapChain && m_hasSwapChain)
  {
    m_hasSwapChain = false;
    m_swapChain->destroy();
  }
}
#include <Gfx/Qt5CompatPop> // clang-format: keep

void Window::render()
{
  if (onUpdate)
    onUpdate();

  if (!m_swapChain)
    return;

  if (!m_hasSwapChain || m_notExposed)
  {
    requestUpdate();
    return;
  }

  if (m_swapChain->currentPixelSize() != m_swapChain->surfacePixelSize()
      || m_newlyExposed)
  {
    resizeSwapChain();
    if (!m_hasSwapChain)
      return;
    m_newlyExposed = false;
  }

  if (m_canRender)
  {
    QRhi::FrameOpResult r = state.rhi->beginFrame(m_swapChain, {});
    if (r == QRhi::FrameOpSwapChainOutOfDate)
    {
      resizeSwapChain();
      if (!m_hasSwapChain)
      {
        requestUpdate();
        return;
      }
      r = state.rhi->beginFrame(m_swapChain);
    }
    if (r != QRhi::FrameOpSuccess)
    {
      requestUpdate();
      return;
    }

    const auto commands = m_swapChain->currentFrameCommandBuffer();
    onRender(*commands);

    state.rhi->endFrame(m_swapChain, {});
  }
  else
  {
    QRhi::FrameOpResult r = state.rhi->beginFrame(m_swapChain, {});
    if (r == QRhi::FrameOpSwapChainOutOfDate)
    {
      resizeSwapChain();
      if (!m_hasSwapChain)
      {
        requestUpdate();
        return;
      }
      r = state.rhi->beginFrame(m_swapChain);
    }
    if (r != QRhi::FrameOpSuccess)
    {
      requestUpdate();
      return;
    }

    auto buf = m_swapChain->currentFrameCommandBuffer();
    auto batch = state.rhi->nextResourceUpdateBatch();
    buf->beginPass(
        m_swapChain->currentFrameRenderTarget(), Qt::black, {1.0f, 0}, batch);
    buf->endPass();

    state.rhi->endFrame(m_swapChain, {});
  }
  requestUpdate();
}

void Window::exposeEvent(QExposeEvent*)
{
  if (!onWindowReady)
  {
    return;
  }
  if (isExposed() && !m_running)
  {
    m_running = true;
    init();
    resizeSwapChain();
  }

  const QSize surfaceSize
      = m_hasSwapChain ? m_swapChain->surfacePixelSize() : QSize();

  if ((!isExposed() || (m_hasSwapChain && surfaceSize.isEmpty())) && m_running)
    m_notExposed = true;

  if (isExposed() && m_running && m_notExposed && !surfaceSize.isEmpty())
  {
    m_notExposed = false;
    m_newlyExposed = true;
  }

  if (isExposed() && !surfaceSize.isEmpty())
    render();
}

void Window::mouseDoubleClickEvent(QMouseEvent* ev)
{
  setWindowState(Qt::WindowState(windowState() ^ Qt::WindowFullScreen));
}

bool Window::event(QEvent* e)
{
  switch (e->type())
  {
    case QEvent::UpdateRequest:
      render();
      break;

    case QEvent::PlatformSurface:
      if (static_cast<QPlatformSurfaceEvent*>(e)->surfaceEventType()
          == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
      {
        releaseSwapChain();
        m_running = false;
        m_hasSwapChain = false;
        m_notExposed = true;
      }

      break;

    default:
      break;
  }

  return QWindow::event(e);
}

}

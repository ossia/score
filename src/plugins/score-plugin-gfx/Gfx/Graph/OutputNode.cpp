#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <QCoreApplication>

#if defined(Q_OS_WIN)
// clang-format off
#include <windows.h>
#include <d3d11.h>
#include <d3d12.h>
// clang-format on
#if __has_include(<QtGui/rhi/qrhi_platform.h>)
#include <QtGui/rhi/qrhi_platform.h>
#endif
#endif

namespace score::gfx
{

OutputNode::OutputNode() { }

OutputNode::~OutputNode() { }

void OutputNode::updateGraphicsAPI(GraphicsApi) { }

void OutputNode::releaseOwnedRenderList()
{
  // Copy before invoking: the Graph is free to rebuild this output from
  // inside the callback, which reassigns m_onReleaseRenderList — destroying
  // the std::function whose operator() is on the stack.
  if(auto cb = m_onReleaseRenderList)
    cb();
}

void OutputNode::handleDeviceLost(const char* output) noexcept
{
  if(m_deviceLost)
    return;

  m_deviceLost = true;

  if(QCoreApplication::closingDown())
    return;

  qCritical().nospace() << "score::gfx::" << output
                        << ": the graphics device was lost while the output was live. "
                           "This output has stopped rendering.";
}

namespace
{
// Whether the native device is gone, asked of the device itself. Qt's D3D
// backends only notice a removal when a swapchain Present or a texture
// creation fails, so a device lost under offscreen frames went unreported:
// every later call failed and the output kept rendering into it.
bool nativeDeviceRemoved(QRhi& rhi) noexcept
{
#if defined(Q_OS_WIN)
  switch(rhi.backend())
  {
    case QRhi::D3D12:
      if(auto* h = static_cast<const QRhiD3D12NativeHandles*>(rhi.nativeHandles());
         h && h->dev)
        return static_cast<ID3D12Device*>(h->dev)->GetDeviceRemovedReason() != S_OK;
      break;
    case QRhi::D3D11:
      if(auto* h = static_cast<const QRhiD3D11NativeHandles*>(rhi.nativeHandles());
         h && h->dev)
        return static_cast<ID3D11Device*>(h->dev)->GetDeviceRemovedReason() != S_OK;
      break;
    default:
      break;
  }
#endif
  (void)rhi;
  return false;
}
}

bool OutputNode::checkDeviceLost(
    const OffscreenFrame& frame, QRhi& rhi, const char* output) noexcept
{
  if(!m_deviceLost
     && (frame.deviceLost() || rhi.isDeviceLost() || nativeDeviceRemoved(rhi)))
    handleDeviceLost(output);
  return m_deviceLost;
}

bool OutputNode::checkDeviceLost(int frameOpResult, QRhi& rhi, const char* output) noexcept
{
  if(!m_deviceLost
     && (frameOpResult == QRhi::FrameOpDeviceLost || rhi.isDeviceLost()
         || nativeDeviceRemoved(rhi)))
    handleDeviceLost(output);
  return m_deviceLost;
}

void OutputNode::setVSyncCallback(std::function<void()>) { }
OutputNodeRenderer::~OutputNodeRenderer() { }

void OutputNodeRenderer::finishFrame(
    RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res)
{
}

GpuResourceRegistry& OutputNode::acquireRegistry()
{
  // Persist-across-rebuild contract: lazy-allocated once per OutputNode.
  // RenderList::init then either calls GpuResourceRegistry::init() (first
  // RL on this OutputNode / first RL after a releaseRegistry()) or reuses
  // the populated state as-is.
  if(!m_registry)
    m_registry = std::make_unique<GpuResourceRegistry>();
  return *m_registry;
}

void OutputNode::releaseRegistry()
{
  // Concrete subclasses MUST call this from destroyOutput() BEFORE the
  // QRhi is torn down. destroyOwned() `delete`s the QRhiBuffer /
  // QRhiTexture / QRhiSampler wrappers directly, so the QRhi must still
  // be alive to honour the QRhiResource destructors.
  if(m_registry)
  {
    m_registry->destroyOwned();
    m_registry.reset();
  }
}

}

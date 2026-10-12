#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <QCoreApplication>

#include <algorithm>
#include <iterator>

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

#if defined(Q_OS_WIN)
const char* dredOpName(D3D12_AUTO_BREADCRUMB_OP op) noexcept
{
  static constexpr const char* names[] = {
      "SetMarker", "BeginEvent", "EndEvent", "DrawInstanced",
      "DrawIndexedInstanced", "ExecuteIndirect", "Dispatch", "CopyBufferRegion",
      "CopyTextureRegion", "CopyResource", "CopyTiles", "ResolveSubresource",
      "ClearRenderTargetView", "ClearUnorderedAccessView", "ClearDepthStencilView",
      "ResourceBarrier", "ExecuteBundle", "Present", "ResolveQueryData",
      "BeginSubmission", "EndSubmission"};
  const auto i = std::size_t(op);
  return i < std::size(names) ? names[i] : "Other";
}
#endif

// With SCORE_GPU_VALIDATION set, ScreenNode turns DRED on for D3D12; print
// what it recorded: the removal reason, every command list that had not
// finished on the GPU with the operations around the last one it completed,
// and the allocations near a page-faulting address.
void logD3D12RemovalInfo(QRhi& rhi) noexcept
{
#if defined(Q_OS_WIN)
  if(rhi.backend() != QRhi::D3D12)
    return;
  auto* h = static_cast<const QRhiD3D12NativeHandles*>(rhi.nativeHandles());
  if(!h || !h->dev)
    return;
  auto* dev = static_cast<ID3D12Device*>(h->dev);
  qCritical("D3D12 device removed, reason 0x%08lx",
            static_cast<unsigned long>(dev->GetDeviceRemovedReason()));

  ID3D12DeviceRemovedExtendedData* dred{};
  if(FAILED(dev->QueryInterface(
         __uuidof(ID3D12DeviceRemovedExtendedData), reinterpret_cast<void**>(&dred))))
    return;

  D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT crumbs{};
  if(SUCCEEDED(dred->GetAutoBreadcrumbsOutput(&crumbs)))
  {
    for(auto* n = crumbs.pHeadAutoBreadcrumbNode; n; n = n->pNext)
    {
      const UINT count = n->BreadcrumbCount;
      const UINT done = n->pLastBreadcrumbValue ? *n->pLastBreadcrumbValue : 0;
      if(done >= count)
        continue; // this command list completed
      qCritical("DRED: command list %p (%s) on queue %p: %u of %u operations "
                "completed",
                (void*)n->pCommandList,
                n->pCommandListDebugNameA ? n->pCommandListDebugNameA : "unnamed",
                (void*)n->pCommandQueue, done, count);
      // The whole history, run-length encoded: the clears, resolves and
      // barriers between draws delimit the passes, which places the failing
      // operation in the frame.
      for(UINT i = 0; i < count;)
      {
        const auto op = n->pCommandHistory[i];
        UINT j = i + 1;
        while(j < count && n->pCommandHistory[j] == op && !(i < done && j >= done)
              && !(i == done))
          j++;
        if(j - i > 1)
          qCritical("DRED:   [%u-%u] %s x%u%s", i, j - 1, dredOpName(op), j - i,
                    (done >= i && done < j) ? "   <-- first operation that did not complete" : "");
        else
          qCritical("DRED:   [%u] %s%s", i, dredOpName(op),
                    i == done ? "   <-- first operation that did not complete" : "");
        i = j;
      }
    }
  }

  D3D12_DRED_PAGE_FAULT_OUTPUT fault{};
  if(SUCCEEDED(dred->GetPageFaultAllocationOutput(&fault)) && fault.PageFaultVA)
  {
    qCritical("DRED: page fault at GPU VA 0x%llx",
              static_cast<unsigned long long>(fault.PageFaultVA));
    for(auto* a = fault.pHeadExistingAllocationNode; a; a = a->pNext)
      qCritical("DRED:   live allocation there: %s (type %d)",
                a->ObjectNameA ? a->ObjectNameA : "unnamed", int(a->AllocationType));
    for(auto* a = fault.pHeadRecentFreedAllocationNode; a; a = a->pNext)
      qCritical("DRED:   recently freed allocation there: %s (type %d)",
                a->ObjectNameA ? a->ObjectNameA : "unnamed", int(a->AllocationType));
  }
  dred->Release();
#else
  (void)rhi;
#endif
}
}

bool OutputNode::checkDeviceLost(
    const OffscreenFrame& frame, QRhi& rhi, const char* output) noexcept
{
  if(!m_deviceLost
     && (frame.deviceLost() || rhi.isDeviceLost() || nativeDeviceRemoved(rhi)))
  {
    logD3D12RemovalInfo(rhi);
    handleDeviceLost(output);
  }
  return m_deviceLost;
}

bool OutputNode::checkDeviceLost(int frameOpResult, QRhi& rhi, const char* output) noexcept
{
  if(!m_deviceLost
     && (frameOpResult == QRhi::FrameOpDeviceLost || rhi.isDeviceLost()
         || nativeDeviceRemoved(rhi)))
  {
    logD3D12RemovalInfo(rhi);
    handleDeviceLost(output);
  }
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

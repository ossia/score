#include <Gfx/Graph/interop/InteropFence.hpp>

#include <QDebug>

#include <QtGui/private/qrhi_p.h>
#include <QtGui/private/qrhigles2_p.h>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

namespace score::gfx::interop
{
namespace
{

// =============================================================================
// D3D11 — no-op (immediate context flush is sufficient)
// =============================================================================

struct InteropFenceD3D11 final : InteropFence
{
  bool m_inited{};
  bool valid() const noexcept override { return m_inited; }

  bool init(QRhi&, CudaP2PContextHandle) override
  {
    m_inited = true;
    return true;
  }
  void release() override { m_inited = false; }
  void signalAfterEncode(QRhiCommandBuffer&, std::uint64_t) override { }
  bool waitOnCuda(std::uint64_t) override { return m_inited; }
};

// =============================================================================
// OpenGL — glFinish once per frame
// =============================================================================

struct InteropFenceGL final : InteropFence
{
  QOpenGLContext* m_ctx{};

  bool valid() const noexcept override { return m_ctx != nullptr; }

  bool init(QRhi& rhi, CudaP2PContextHandle) override
  {
    auto* nh
        = static_cast<const QRhiGles2NativeHandles*>(rhi.nativeHandles());
    if(!nh || !nh->context)
      return false;
    m_ctx = nh->context;
    return true;
  }
  void release() override { m_ctx = nullptr; }
  void signalAfterEncode(QRhiCommandBuffer&, std::uint64_t) override { }
  bool waitOnCuda(std::uint64_t) override
  {
    if(!m_ctx)
      return false;
    if(auto* f = m_ctx->extraFunctions())
      f->glFinish();
    return true;
  }
};

// =============================================================================
// Vulkan — stub (would use VK_KHR_timeline_semaphore + cuda_p2p_import_vulkan_semaphore)
// =============================================================================

struct InteropFenceVulkanStub final : InteropFence
{
  bool valid() const noexcept override { return false; }

  bool init(QRhi&, CudaP2PContextHandle) override
  {
    qDebug() << "InteropFence(Vulkan): stub — depends on the Vulkan tier-3 "
                "output buffer path landing first; see "
                "RdmaInteropVulkanTier3.hpp for the design.";
    return false;
  }
  void release() override { }
  void signalAfterEncode(QRhiCommandBuffer&, std::uint64_t) override { }
  bool waitOnCuda(std::uint64_t) override { return false; }
};

// =============================================================================
// D3D12 — stub
// =============================================================================

struct InteropFenceD3D12Stub final : InteropFence
{
  bool valid() const noexcept override { return false; }

  bool init(QRhi&, CudaP2PContextHandle) override
  {
    qDebug() << "InteropFence(D3D12): stub — needs ID3D12Fence shared "
                "with CUDA, blocked by the same QRhi D3D12 SHARED-heap "
                "limitation as the buffer ring.";
    return false;
  }
  void release() override { }
  void signalAfterEncode(QRhiCommandBuffer&, std::uint64_t) override { }
  bool waitOnCuda(std::uint64_t) override { return false; }
};

} // namespace

std::unique_ptr<InteropFence> makeInteropFence(QRhi& rhi)
{
  switch(rhi.backend())
  {
    case QRhi::D3D11:
      return std::make_unique<InteropFenceD3D11>();
    case QRhi::OpenGLES2:
      return std::make_unique<InteropFenceGL>();
    case QRhi::Vulkan:
      return std::make_unique<InteropFenceVulkanStub>();
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    case QRhi::D3D12:
      return std::make_unique<InteropFenceD3D12Stub>();
#endif
    default:
      qWarning() << "makeInteropFence: unsupported backend" << rhi.backend();
      return std::make_unique<InteropFenceVulkanStub>(); // returns invalid()
  }
}

} // namespace score::gfx::interop

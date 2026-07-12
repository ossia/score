#include <Gfx/Graph/interop/InteropFence.hpp>

#include <score/gfx/Vulkan.hpp>

#include <QDebug>

#include <QtGui/private/qrhi_p.h>
#include <QtGui/private/qrhigles2_p.h>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

#if QT_HAS_VULKAN
#include <QtGui/private/qrhivulkan_p.h>

#include <QVulkanFunctions>
#include <QVulkanInstance>

#include <Gfx/Graph/interop/VkCudaSemaphore.hpp>

#if defined(_WIN32)
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#endif
#endif

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

  bool init(QRhi&, CudaInteropContextHandle) override
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

  bool init(QRhi& rhi, CudaInteropContextHandle) override
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
// Vulkan — stub (used when Qt is built without Vulkan support)
// =============================================================================

struct InteropFenceVulkanStub final : InteropFence
{
  bool valid() const noexcept override { return false; }

  bool init(QRhi&, CudaInteropContextHandle) override
  {
    qDebug() << "InteropFence(Vulkan): stub — Qt built without Vulkan support.";
    return false;
  }
  void release() override { }
  void signalAfterEncode(QRhiCommandBuffer&, std::uint64_t) override { }
  bool waitOnCuda(std::uint64_t) override { return false; }
};

#if QT_HAS_VULKAN && QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
// =============================================================================
// Vulkan — real: a small ring of BINARY VkSemaphores exported to CUDA.
//
// signalAfterEncode() (inside the offscreen frame) asks QRhi to signal
// m_vkSem[value % kRing] at its next queue submit via setQueueSubmitParams.
// endOffscreenFrame() then performs that submit, signalling the binary
// semaphore once the encoder + copyTexture are done on the GPU. waitOnCuda()
// (after endOffscreenFrame, before the array->buffer copy) schedules a CUDA-
// stream wait on the matching imported semaphore, so the peer/CUDA copy never
// reads a half-written VkImage. The value is a monotonic frame counter; for a
// binary semaphore only the ring index matters (CUDA waits with value 0).
//
// Requires the VkDevice to have VK_KHR_external_semaphore +
// VK_KHR_external_semaphore_win32 (/_fd) enabled — see ScreenNode.cpp /
// VulkanVideoDevice.hpp. If those are missing, vkCreateSemaphore with the
// export info fails and init() returns false → caller uses its finish()
// fallback. No partial state is ever left behind.
// =============================================================================

struct InteropFenceVulkan final : InteropFence
{
  static constexpr int kRing = 3;

  QRhi* m_rhi{};
  CudaInteropContextHandle m_cudaCtx{};

  // Reuse the exportable-semaphore + CUDA-import helper (in binary mode)
  // instead of re-rolling vkCreateSemaphore / export / import here.
  vkinterop::VkCudaTimelineSemaphore m_sem[kRing];
  VkSemaphore m_vk[kRing]{}; // stable storage for setQueueSubmitParams pointers
  bool m_ok{};

  bool valid() const noexcept override { return m_ok; }

  bool init(QRhi& rhi, CudaInteropContextHandle cudaCtx) override
  {
    release();
    if(rhi.backend() != QRhi::Vulkan || !cudaCtx)
      return false;

    auto* nh = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
    if(!nh || nh->dev == VK_NULL_HANDLE || !nh->inst)
      return false;

    vkinterop::VulkanCtx vk{};
    vk.instance = nh->inst->vkInstance();
    vk.physDev = nh->physDev;
    vk.dev = nh->dev;
    vk.qInst = nh->inst;

    for(int i = 0; i < kRing; ++i)
    {
      if(!m_sem[i].create(vk, cudaCtx, /*initialValue=*/0, /*binary=*/true))
      {
        release();
        return false;
      }
      m_vk[i] = m_sem[i].vk();
    }

    m_rhi = &rhi;
    m_cudaCtx = cudaCtx;
    m_ok = true;
    return true;
  }

  void signalAfterEncode(QRhiCommandBuffer&, std::uint64_t value) override
  {
    if(!m_ok)
      return;
    const int i = int(value % kRing);
    // QRhi copies these fields at set-time and clears them after each submit, so
    // this must run every frame. m_vk[i] is stable member storage.
    QRhiVulkanQueueSubmitParams params{};
    params.signalSemaphoreCount = 1;
    params.signalSemaphores = &m_vk[i];
    m_rhi->setQueueSubmitParams(&params);
  }

  bool waitOnCuda(std::uint64_t value) override
  {
    if(!m_ok)
      return false;
    const int i = int(value % kRing);
    // Binary semaphore: CUDA ignores the value for OPAQUE types → pass 0.
    return cuda_interop_wait_semaphore(m_cudaCtx, m_sem[i].cuda(), 0)
           == CUDA_INTEROP_SUCCESS;
  }

  void release() override
  {
    for(int i = 0; i < kRing; ++i)
    {
      m_sem[i].destroy();
      m_vk[i] = VK_NULL_HANDLE;
    }
    m_rhi = nullptr;
    m_cudaCtx = nullptr;
    m_ok = false;
  }
};
#endif // QT_HAS_VULKAN

// =============================================================================
// D3D12 — stub
// =============================================================================

struct InteropFenceD3D12Stub final : InteropFence
{
  bool valid() const noexcept override { return false; }

  bool init(QRhi&, CudaInteropContextHandle) override
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
#if QT_HAS_VULKAN && QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
      return std::make_unique<InteropFenceVulkan>();
#else
      return std::make_unique<InteropFenceVulkanStub>();
#endif
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

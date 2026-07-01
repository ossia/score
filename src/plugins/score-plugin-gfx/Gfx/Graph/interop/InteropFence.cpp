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
// Vulkan — stub (used when Qt is built without Vulkan support)
// =============================================================================

struct InteropFenceVulkanStub final : InteropFence
{
  bool valid() const noexcept override { return false; }

  bool init(QRhi&, CudaP2PContextHandle) override
  {
    qDebug() << "InteropFence(Vulkan): stub — Qt built without Vulkan support.";
    return false;
  }
  void release() override { }
  void signalAfterEncode(QRhiCommandBuffer&, std::uint64_t) override { }
  bool waitOnCuda(std::uint64_t) override { return false; }
};

#if QT_HAS_VULKAN
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

#if defined(_WIN32)
  static constexpr VkExternalSemaphoreHandleTypeFlagBits kExportType
      = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
  static constexpr VkExternalSemaphoreHandleTypeFlagBits kExportType
      = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

  QRhi* m_rhi{};
  QVulkanInstance* m_inst{};
  QVulkanDeviceFunctions* m_df{};
  VkDevice m_dev{VK_NULL_HANDLE};
  CudaP2PContextHandle m_cudaCtx{};

  VkSemaphore m_vkSem[kRing]{};
  CudaP2PSemaphoreHandle m_cudaSem[kRing]{};
#if defined(_WIN32)
  HANDLE m_osHandle[kRing]{};
#endif
  bool m_ok{};

  bool valid() const noexcept override { return m_ok; }

  bool init(QRhi& rhi, CudaP2PContextHandle cudaCtx) override
  {
    release();
    if(rhi.backend() != QRhi::Vulkan || !cudaCtx)
      return false;

    auto* nh = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
    if(!nh || nh->dev == VK_NULL_HANDLE || !nh->inst)
      return false;

    m_rhi = &rhi;
    m_dev = nh->dev;
    m_inst = nh->inst;
    m_cudaCtx = cudaCtx;
    m_df = m_inst->deviceFunctions(m_dev);
    if(!m_df)
      return false;

    // Resolve the platform export fn (device-level dispatch via the instance).
#if defined(_WIN32)
    auto exportFn = reinterpret_cast<PFN_vkGetSemaphoreWin32HandleKHR>(
        m_inst->getInstanceProcAddr("vkGetSemaphoreWin32HandleKHR"));
#else
    auto exportFn = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(
        m_inst->getInstanceProcAddr("vkGetSemaphoreFdKHR"));
#endif
    if(!exportFn)
    {
      qDebug() << "InteropFence(Vulkan): semaphore export fn unresolved "
                  "(VK_KHR_external_semaphore_* not enabled?)";
      release();
      return false;
    }

    for(int i = 0; i < kRing; ++i)
    {
      VkExportSemaphoreCreateInfo expCi{};
      expCi.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
      expCi.handleTypes = kExportType;

      // Binary semaphore: no VkSemaphoreTypeCreateInfo (default is BINARY).
      VkSemaphoreCreateInfo ci{};
      ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      ci.pNext = &expCi;

      if(m_df->vkCreateSemaphore(m_dev, &ci, nullptr, &m_vkSem[i]) != VK_SUCCESS
         || m_vkSem[i] == VK_NULL_HANDLE)
      {
        qDebug() << "InteropFence(Vulkan): vkCreateSemaphore(exportable) failed";
        release();
        return false;
      }

      void* osHandle = nullptr;
#if defined(_WIN32)
      VkSemaphoreGetWin32HandleInfoKHR gi{};
      gi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
      gi.semaphore = m_vkSem[i];
      gi.handleType = kExportType;
      HANDLE h = nullptr;
      if(exportFn(m_dev, &gi, &h) != VK_SUCCESS || !h)
      {
        qDebug() << "InteropFence(Vulkan): vkGetSemaphoreWin32HandleKHR failed";
        release();
        return false;
      }
      m_osHandle[i] = h;
      osHandle = h;
#else
      VkSemaphoreGetFdInfoKHR gi{};
      gi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
      gi.semaphore = m_vkSem[i];
      gi.handleType = kExportType;
      int fd = -1;
      if(exportFn(m_dev, &gi, &fd) != VK_SUCCESS || fd < 0)
      {
        qDebug() << "InteropFence(Vulkan): vkGetSemaphoreFdKHR failed";
        release();
        return false;
      }
      osHandle = reinterpret_cast<void*>(static_cast<intptr_t>(fd));
#endif

      if(cuda_p2p_import_vulkan_semaphore_binary(m_cudaCtx, osHandle, &m_cudaSem[i])
             != CUDA_P2P_SUCCESS
         || !m_cudaSem[i])
      {
        qDebug() << "InteropFence(Vulkan): cuda_p2p_import_vulkan_semaphore_binary "
                    "failed";
        release();
        return false;
      }
    }

    m_ok = true;
    return true;
  }

  void signalAfterEncode(QRhiCommandBuffer&, std::uint64_t value) override
  {
    if(!m_ok)
      return;
    const int i = int(value % kRing);
    // QRhi copies these fields into its internal submit state at set-time and
    // clears them after each submit, so this must run every frame. The
    // signalSemaphores pointer targets a member array (stays valid).
    QRhiVulkanQueueSubmitParams params{};
    params.waitSemaphoreCount = 0;
    params.waitSemaphores = nullptr;
    params.signalSemaphoreCount = 1;
    params.signalSemaphores = &m_vkSem[i];
    params.presentWaitSemaphoreCount = 0;
    params.presentWaitSemaphores = nullptr;
    m_rhi->setQueueSubmitParams(&params);
  }

  bool waitOnCuda(std::uint64_t value) override
  {
    if(!m_ok)
      return false;
    const int i = int(value % kRing);
    // Binary semaphore: value is ignored by CUDA for OPAQUE types → pass 0.
    return cuda_p2p_wait_semaphore(m_cudaCtx, m_cudaSem[i], 0)
           == CUDA_P2P_SUCCESS;
  }

  void release() override
  {
    for(int i = 0; i < kRing; ++i)
    {
      if(m_cudaCtx && m_cudaSem[i])
        cuda_p2p_release_semaphore(m_cudaCtx, m_cudaSem[i]);
      m_cudaSem[i] = nullptr;

      if(m_df && m_dev != VK_NULL_HANDLE && m_vkSem[i] != VK_NULL_HANDLE)
        m_df->vkDestroySemaphore(m_dev, m_vkSem[i], nullptr);
      m_vkSem[i] = VK_NULL_HANDLE;

#if defined(_WIN32)
      if(m_osHandle[i])
        CloseHandle(m_osHandle[i]);
      m_osHandle[i] = nullptr;
#endif
    }
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
#if QT_HAS_VULKAN
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

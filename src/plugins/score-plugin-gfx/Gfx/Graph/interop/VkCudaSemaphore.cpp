#include "VkCudaSemaphore.hpp"

#if QT_HAS_VULKAN

#include <QDebug>
#include <QVulkanFunctions>
#include <QVulkanInstance>

#if defined(_WIN32)
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#endif

namespace score::gfx::vkinterop
{

namespace
{

#if defined(_WIN32)
constexpr VkExternalSemaphoreHandleTypeFlagBits kExportType
    = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
constexpr VkExternalSemaphoreHandleTypeFlagBits kExportType
    = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

} // namespace

VkCudaTimelineSemaphore::~VkCudaTimelineSemaphore()
{
  destroy();
}

VkCudaTimelineSemaphore::VkCudaTimelineSemaphore(
    VkCudaTimelineSemaphore&& other) noexcept
    : m_vk(other.m_vk)
    , m_cudaCtx(other.m_cudaCtx)
    , m_vkSem(other.m_vkSem)
    , m_cudaSem(other.m_cudaSem)
{
  other.m_vk = {};
  other.m_cudaCtx = nullptr;
  other.m_vkSem = VK_NULL_HANDLE;
  other.m_cudaSem = nullptr;
}

VkCudaTimelineSemaphore&
VkCudaTimelineSemaphore::operator=(VkCudaTimelineSemaphore&& other) noexcept
{
  if(this != &other)
  {
    destroy();
    m_vk = other.m_vk;
    m_cudaCtx = other.m_cudaCtx;
    m_vkSem = other.m_vkSem;
    m_cudaSem = other.m_cudaSem;
    other.m_vk = {};
    other.m_cudaCtx = nullptr;
    other.m_vkSem = VK_NULL_HANDLE;
    other.m_cudaSem = nullptr;
  }
  return *this;
}

bool VkCudaTimelineSemaphore::create(
    const VulkanCtx& vk, CudaInteropContextHandle cudaCtx, uint64_t initialValue,
    bool binary)
{
  destroy();
  if(!cudaCtx || vk.dev == VK_NULL_HANDLE || !vk.qInst)
    return false;

  m_vk = vk;
  m_cudaCtx = cudaCtx;

  auto* df = vk.qInst->deviceFunctions(vk.dev);
  if(!df)
    return false;

  // 1. Build a VkSemaphoreTypeCreateInfo for VK_SEMAPHORE_TYPE_TIMELINE
  //    chained with a VkExportSemaphoreCreateInfo for the OS handle
  //    type. Timeline semaphores require Vulkan 1.2 or VK_KHR_timeline_semaphore.
  VkExportSemaphoreCreateInfo expCi{};
  expCi.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
  expCi.handleTypes = kExportType;

  // Timeline chains a VkSemaphoreTypeCreateInfo; binary chains only the export
  // info (a default VkSemaphoreCreateInfo is already BINARY).
  VkSemaphoreTypeCreateInfo typeCi{};
  typeCi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
  typeCi.pNext = &expCi;
  typeCi.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  typeCi.initialValue = initialValue;

  VkSemaphoreCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  ci.pNext = binary ? static_cast<const void*>(&expCi)
                    : static_cast<const void*>(&typeCi);

  if(df->vkCreateSemaphore(vk.dev, &ci, nullptr, &m_vkSem) != VK_SUCCESS
     || m_vkSem == VK_NULL_HANDLE)
  {
    qWarning() << "VkCudaTimelineSemaphore: vkCreateSemaphore failed";
    destroy();
    return false;
  }

  // 2. Resolve the platform-appropriate exporter (must use the device-
  //    level dispatch since the function is part of an extension
  //    promoted to core in 1.1).
  auto* vf = vk.qInst->functions();
  (void)vf;
  void* osHandle = nullptr;
#if defined(_WIN32)
  using PFN_vkGetSemaphoreWin32HandleKHR_t
      = VkResult(VKAPI_PTR*)(VkDevice, const VkSemaphoreGetWin32HandleInfoKHR*,
                              HANDLE*);
  auto pfn = reinterpret_cast<PFN_vkGetSemaphoreWin32HandleKHR_t>(
      vk.qInst->getInstanceProcAddr("vkGetSemaphoreWin32HandleKHR"));
  if(!pfn)
  {
    qWarning() << "VkCudaTimelineSemaphore: vkGetSemaphoreWin32HandleKHR "
                  "unresolved";
    destroy();
    return false;
  }
  VkSemaphoreGetWin32HandleInfoKHR info{};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
  info.semaphore = m_vkSem;
  info.handleType = kExportType;
  HANDLE h = nullptr;
  if(pfn(vk.dev, &info, &h) != VK_SUCCESS || !h)
  {
    qWarning() << "VkCudaTimelineSemaphore: vkGetSemaphoreWin32HandleKHR "
                  "returned null";
    destroy();
    return false;
  }
  osHandle = h;
#else
  using PFN_vkGetSemaphoreFdKHR_t
      = VkResult(VKAPI_PTR*)(VkDevice, const VkSemaphoreGetFdInfoKHR*, int*);
  auto pfn = reinterpret_cast<PFN_vkGetSemaphoreFdKHR_t>(
      vk.qInst->getInstanceProcAddr("vkGetSemaphoreFdKHR"));
  if(!pfn)
  {
    qWarning() << "VkCudaTimelineSemaphore: vkGetSemaphoreFdKHR unresolved";
    destroy();
    return false;
  }
  VkSemaphoreGetFdInfoKHR info{};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
  info.semaphore = m_vkSem;
  info.handleType = kExportType;
  int fd = -1;
  if(pfn(vk.dev, &info, &fd) != VK_SUCCESS || fd < 0)
  {
    qWarning() << "VkCudaTimelineSemaphore: vkGetSemaphoreFdKHR returned -1";
    destroy();
    return false;
  }
  osHandle = reinterpret_cast<void*>(static_cast<intptr_t>(fd));
#endif

  // 3. CUDA-side import via the existing CudaInterop entry points (binary vs
  //    timeline handle type).
  const auto importRes
      = binary
            ? ::cuda_interop_import_vulkan_semaphore_binary(cudaCtx, osHandle, &m_cudaSem)
            : ::cuda_interop_import_vulkan_semaphore(cudaCtx, osHandle, &m_cudaSem);
  if(importRes != CUDA_INTEROP_SUCCESS)
  {
    qWarning() << "VkCudaTimelineSemaphore: CUDA semaphore import failed";
    destroy();
    return false;
  }

  // On Win32 the exported HANDLE is ours; CUDA duplicates it during import, so
  // release our reference now to avoid a per-semaphore handle leak. On Linux the
  // OPAQUE_FD ownership transfers to CUDA — do not close the fd.
#if defined(_WIN32)
  CloseHandle(h);
#endif
  return true;
}

void VkCudaTimelineSemaphore::destroy() noexcept
{
  if(m_cudaCtx && m_cudaSem)
  {
    ::cuda_interop_release_semaphore(m_cudaCtx, m_cudaSem);
    m_cudaSem = nullptr;
  }
  if(m_vk.dev != VK_NULL_HANDLE && m_vkSem != VK_NULL_HANDLE && m_vk.qInst)
  {
    if(auto* df = m_vk.qInst->deviceFunctions(m_vk.dev))
      df->vkDestroySemaphore(m_vk.dev, m_vkSem, nullptr);
  }
  m_vkSem = VK_NULL_HANDLE;
  m_cudaCtx = nullptr;
  m_vk = {};
}

bool VkCudaTimelineSemaphore::valid() const noexcept
{
  return m_vkSem != VK_NULL_HANDLE && m_cudaSem != nullptr;
}

bool VkCudaTimelineSemaphore::hostSignal(uint64_t value)
{
  if(!valid() || !m_vk.qInst)
    return false;
  auto* df = m_vk.qInst->deviceFunctions(m_vk.dev);
  if(!df)
    return false;

  VkSemaphoreSignalInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
  si.semaphore = m_vkSem;
  si.value = value;
  return df->vkSignalSemaphore(m_vk.dev, &si) == VK_SUCCESS;
}

} // namespace score::gfx::vkinterop

#endif // QT_HAS_VULKAN

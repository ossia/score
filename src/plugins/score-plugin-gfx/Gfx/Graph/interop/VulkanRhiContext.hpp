#pragma once

/**
 * @file VulkanRhiContext.hpp
 * @brief Acquire the raw Vulkan handles (instance/physDev/device/queue/family)
 *        behind a Vulkan QRhi, for the RDMA GPU-direct interop paths.
 *
 * The RDMA Vulkan capture + output shims all need the same handles out of
 * QRhiVulkanNativeHandles + score's shared QVulkanInstance to drive the
 * exportable-image / CUDA-import machinery. This block was copied verbatim into
 * each; it now lives here.
 *
 * NOTE (M3/M4 scope): this is the *genuine* common denominator of the AJA and
 * Deltacast Vulkan RDMA paths. Their transfer machinery does NOT converge — AJA
 * uses per-slot LINEAR images imported as flat CUDA buffers (cuda_interop copy
 * dtod-2d) with an exportable-VkBuffer bounce pinned via DMABufferLock, while
 * Deltacast uses a single OPTIMAL image imported as a CUDA array
 * (cuda_interop copy buffer<->array) backed by RdmaGpuBuffer. These are deliberate,
 * correctness-driven choices (AJA notes an OPTIMAL-image-as-array "scrambles
 * per-tile on this driver"), so a shared *transfer* base is a redesign, not a
 * mechanical move, and is deferred until the Vulkan RDMA paths can be
 * hardware-validated together.
 */

#include <Gfx/Graph/interop/VkExternalMemoryHelpers.hpp>

#include <score/gfx/Vulkan.hpp>

#if QT_HAS_VULKAN

#include <QtGui/private/qrhivulkan_p.h>

#include <QVulkanInstance>

class QRhi;
class QVulkanDeviceFunctions;

namespace score::gfx::interop
{

/// Raw Vulkan handles borrowed from a Vulkan QRhi backend.
struct VulkanRhiContext
{
  score::gfx::vkinterop::VulkanCtx vk{};
  QVulkanDeviceFunctions* devFuncs{};
  VkQueue gfxQueue{VK_NULL_HANDLE};
  int gfxFamily{-1};
};

/// Fill @p out from @p rhi's native Vulkan handles + score's shared
/// QVulkanInstance. Returns false if @p rhi is not a Vulkan QRhi or any handle
/// is unavailable (same guard the standalone shims used).
inline bool acquireVulkanRhiContext(QRhi* rhi, VulkanRhiContext& out) noexcept
{
  if(!rhi || rhi->backend() != QRhi::Vulkan)
    return false;
  auto* h = static_cast<const QRhiVulkanNativeHandles*>(rhi->nativeHandles());
  QVulkanInstance* qInst = score::gfx::staticVulkanInstance(false);
  if(!h || !h->dev || !h->physDev || !qInst)
    return false;
  out.vk = {qInst->vkInstance(), h->physDev, h->dev, qInst};
  out.devFuncs = qInst->deviceFunctions(h->dev);
  out.gfxQueue = h->gfxQueue;
  out.gfxFamily = h->gfxQueueFamilyIdx;
  if(!out.devFuncs || !out.gfxQueue || out.gfxFamily < 0)
    return false;
  return true;
}

} // namespace score::gfx::interop

#endif // QT_HAS_VULKAN

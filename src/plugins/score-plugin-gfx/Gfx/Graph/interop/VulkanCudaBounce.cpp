#include <Gfx/Graph/interop/VulkanCudaBounce.hpp>

#if defined(SCORE_HAS_VULKAN_CUDA_BOUNCE)

#include <score/gfx/Vulkan.hpp>

#include <QDebug>
#include <QVulkanInstance>

#if __has_include(<QtGui/rhi/qrhi_platform.h>)
#include <QtGui/rhi/qrhi_platform.h>
#else
#include <QtGui/private/qrhi_p.h>
#include <QtGui/private/qrhivulkan_p.h>
#endif

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace score::gfx::interop
{

bool VulkanCudaBounce::init(const VulkanCudaBounceConfig& cfg)
{
  release();
  if(!cfg.rhi || cfg.rhi->backend() != QRhi::Vulkan || cfg.slotCount < 1
     || cfg.frameBytes == 0)
  {
    qWarning() << cfg.debugName << ": bad config";
    return false;
  }

  if(cuda_p2p_init(&m_p2p) != CUDA_P2P_SUCCESS || !m_p2p)
  {
    qWarning() << cfg.debugName << ": CUDA bridge init failed";
    return false;
  }

  // --- Vulkan side: device/instance from the QRhi.
  auto* h
      = static_cast<const QRhiVulkanNativeHandles*>(cfg.rhi->nativeHandles());
  // score always builds its Vulkan QRhi from staticVulkanInstance()
  // (Qt 6.4's QRhiVulkanNativeHandles has no inst member to ask instead).
  QVulkanInstance* qInst = score::gfx::staticVulkanInstance(false);
  if(!h || !h->dev || !h->physDev || !qInst)
  {
    qWarning() << cfg.debugName << ": no Vulkan native handles";
    release();
    return false;
  }
  m_vk = vkinterop::VulkanCtx{qInst->vkInstance(), h->physDev, h->dev, qInst};

  m_fnCopyBuffer
      = reinterpret_cast<void*>(qInst->getInstanceProcAddr("vkCmdCopyBuffer"));
  m_fnCopyBufferToImage = reinterpret_cast<void*>(
      qInst->getInstanceProcAddr("vkCmdCopyBufferToImage"));
  m_fnPipelineBarrier = reinterpret_cast<void*>(
      qInst->getInstanceProcAddr("vkCmdPipelineBarrier"));
  if(!m_fnCopyBuffer || !m_fnCopyBufferToImage || !m_fnPipelineBarrier)
  {
    qWarning() << cfg.debugName << ": vk entry points missing";
    release();
    return false;
  }

  m_frameBytes = cfg.frameBytes;
  m_slots.reserve(std::size_t(cfg.slotCount));
  for(int i = 0; i < cfg.slotCount; ++i)
  {
    Slot s{};
    // 1. Vulkan-owned exportable buffer (VkExportMemoryAllocateInfo). This
    //    is the SUPPORTED interop direction: Vulkan exports, CUDA imports.
    //    (A CUDA-VMM fd imported into Vulkan allocates fine but does not
    //    alias — writes land in private memory. Measured, not theoretical.)
    vkinterop::ExternalBufferDesc desc{};
    desc.size = cfg.frameBytes;
    desc.usage
        = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    desc.handleType = vkinterop::kOpaqueHandleType;
    desc.dedicated = true;
    auto created = vkinterop::createExportableBuffer(m_vk, desc);
    if(!created)
    {
      qWarning() << cfg.debugName << ": exportable VkBuffer failed at" << i;
      release();
      return false;
    }
    s.vk = *created;

    auto handle
        = vkinterop::exportMemoryHandle(m_vk, s.vk.memory, desc.handleType);
    if(!handle || !handle->isValid())
    {
      qWarning() << cfg.debugName << ": memory export failed at" << i;
      vkinterop::destroyExternal(m_vk, s.vk);
      release();
      return false;
    }

    // 2. CUDA view of the exportable buffer (consumes the fd on success).
    if(cuda_p2p_import_vulkan_buffer(
           m_p2p, handle->osHandle(), s.vk.size, &s.vkCudaPtr, &s.vkCudaHandle)
           != CUDA_P2P_SUCCESS
       || !s.vkCudaPtr)
    {
      qWarning() << cfg.debugName
                 << ": CUDA import failed at" << i
                 << "— if multiple GPUs are present, the Vulkan physical "
                    "device must be the CUDA device (steer QRhi with "
                    "QT_VK_PHYSICAL_DEVICE_INDEX).";
      vkinterop::destroyExternal(m_vk, s.vk);
      release();
      return false;
    }

    // 3. Pinned CUDA bounce the vendor DMAs (cuMemAlloc + SYNC_MEMOPS).
    if(cuda_p2p_alloc_buffer(m_p2p, cfg.frameBytes, &s.bouncePtr)
           != CUDA_P2P_SUCCESS
       || !s.bouncePtr)
    {
      qWarning() << cfg.debugName << ": bounce alloc failed at" << i;
      cuda_p2p_release_buffer(m_p2p, s.vkCudaHandle);
      vkinterop::destroyExternal(m_vk, s.vk);
      release();
      return false;
    }
    m_slots.push_back(s);
  }
  qDebug() << cfg.debugName << ": ready," << m_slots.size() << "slots of"
           << qulonglong(cfg.frameBytes) << "bytes";
  return true;
}

void VulkanCudaBounce::release()
{
  for(auto& s : m_slots)
  {
    if(s.bouncePtr && m_p2p)
      cuda_p2p_free_buffer(m_p2p, s.bouncePtr);
    if(s.vkCudaHandle && m_p2p)
      cuda_p2p_release_buffer(m_p2p, s.vkCudaHandle);
    if(s.vk.buffer || s.vk.memory)
      vkinterop::destroyExternal(m_vk, s.vk);
  }
  m_slots.clear();
  if(m_p2p)
  {
    cuda_p2p_shutdown(m_p2p);
    m_p2p = nullptr;
  }
  m_vk = {};
  m_frameBytes = 0;
}

bool VulkanCudaBounce::flushToBounce(std::size_t i, std::size_t bytes)
{
  if(i >= m_slots.size() || bytes > m_frameBytes)
    return false;
  return cuda_p2p_copy_dtod(
             m_p2p, m_slots[i].bouncePtr, m_slots[i].vkCudaPtr, bytes)
         == CUDA_P2P_SUCCESS;
}

bool VulkanCudaBounce::flushFromBounce(std::size_t i, std::size_t bytes)
{
  if(i >= m_slots.size() || bytes > m_frameBytes)
    return false;
  return cuda_p2p_copy_dtod(
             m_p2p, m_slots[i].vkCudaPtr, m_slots[i].bouncePtr, bytes)
         == CUDA_P2P_SUCCESS;
}

void VulkanCudaBounce::debugPeek(std::size_t i, const char* tag)
{
  if(i >= m_slots.size() || !m_p2p)
    return;
  unsigned char b[16]{};
  if(cuda_p2p_download_buffer(m_p2p, b, m_slots[i].bouncePtr, sizeof(b))
     == CUDA_P2P_SUCCESS)
    qDebug() << tag << "slot" << int(i) << "bytes:" << b[0] << b[1] << b[2]
             << b[3] << b[4] << b[5] << b[6] << b[7];
  else
    qDebug() << tag << "peek failed";
}

bool VulkanCudaBounce::recordCopyToSlot(
    QRhiCommandBuffer& cb, void* srcVkBuffer, std::size_t bytes,
    std::size_t i)
{
  if(i >= m_slots.size() || !srcVkBuffer || bytes > m_frameBytes)
    return false;
  auto* native
      = static_cast<const QRhiVulkanCommandBufferNativeHandles*>(cb.nativeHandles());
  if(!native || !native->commandBuffer)
    return false;

  auto pBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(m_fnPipelineBarrier);
  auto pCopy = reinterpret_cast<PFN_vkCmdCopyBuffer>(m_fnCopyBuffer);

  cb.beginExternal();
  // Encoder compute writes -> transfer read.
  VkMemoryBarrier pre{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                      VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT};
  pBarrier(
      native->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &pre, 0, nullptr, 0, nullptr);
  VkBufferCopy region{0, 0, VkDeviceSize(bytes)};
  pCopy(
      native->commandBuffer, VkBuffer(srcVkBuffer), m_slots[i].vk.buffer, 1,
      &region);
  // Make the write available before the external (PCIe P2P) consumer; the
  // caller's QRhi::finish() provides the host-side ordering.
  VkMemoryBarrier post{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                       VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT};
  pBarrier(
      native->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 1, &post, 0, nullptr, 0, nullptr);
  cb.endExternal();
  return true;
}

bool VulkanCudaBounce::recordCopySlotToTexture(
    QRhiCommandBuffer& cb, QRhiTexture* dst, int width, int height,
    int texelBytes, std::size_t i)
{
  if(i >= m_slots.size() || !dst || width <= 0 || height <= 0)
    return false;
  if(std::size_t(width) * height * texelBytes > m_frameBytes)
    return false;
  auto* native
      = static_cast<const QRhiVulkanCommandBufferNativeHandles*>(cb.nativeHandles());
  if(!native || !native->commandBuffer)
    return false;

  const auto nat = dst->nativeTexture();
  // Texture convention (unlike buffers): object IS the 64-bit VkImage.
  VkImage image = VkImage(nat.object);
  if(image == VK_NULL_HANDLE)
    return false;

  auto pBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(m_fnPipelineBarrier);
  auto pCopy
      = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(m_fnCopyBufferToImage);

  cb.beginExternal();
  VkImageMemoryBarrier toDst{};
  toDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  toDst.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  toDst.oldLayout = VkImageLayout(nat.layout);
  toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  toDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  toDst.image = image;
  toDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  pBarrier(
      native->commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDst);

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;   // tightly packed
  region.bufferImageHeight = 0; // tightly packed
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageExtent = {uint32_t(width), uint32_t(height), 1};
  pCopy(
      native->commandBuffer, m_slots[i].vk.buffer, image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  VkImageMemoryBarrier toRead = toDst;
  toRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  pBarrier(
      native->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
          | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      0, 0, nullptr, 0, nullptr, 1, &toRead);
  cb.endExternal();

  dst->setNativeLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  return true;
}

} // namespace score::gfx::interop

#endif

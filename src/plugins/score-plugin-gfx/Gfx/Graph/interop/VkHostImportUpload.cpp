#include "VkHostImportUpload.hpp"

#include <QtGui/private/qrhi_p.h>

#include <cstdlib>

#if QT_CONFIG(vulkan)
#include <QVulkanFunctions>
#include <QVulkanInstance>
#include <QtGui/private/qrhivulkan_p.h>

#include <score/gfx/Vulkan.hpp>
#endif

#include <QDebug>

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace score::gfx::interop
{

void* alignedSlotAlloc(std::size_t bytes, std::size_t alignment)
{
  if(alignment < sizeof(void*))
    alignment = sizeof(void*);
  const std::size_t rounded = ((bytes + alignment - 1) / alignment) * alignment;
#if defined(_WIN32)
  return _aligned_malloc(rounded, alignment);
#else
  return std::aligned_alloc(alignment, rounded);
#endif
}

void alignedSlotFree(void* p)
{
#if defined(_WIN32)
  _aligned_free(p);
#else
  std::free(p);
#endif
}

#if QT_CONFIG(vulkan)

struct VkHostImportUpload::Impl
{
  QRhi* rhi{};
  VkDevice dev{};
  VkPhysicalDevice phys{};
  QVulkanInstance* inst{};
  QVulkanDeviceFunctions* df{};
  PFN_vkGetMemoryHostPointerPropertiesEXT getHostPtrProps{};
  std::vector<VkDeviceMemory> mems;
  std::size_t importedBytes{};
};

namespace
{
struct VulkanBits
{
  VkDevice dev{};
  VkPhysicalDevice phys{};
  QVulkanInstance* inst{};
  bool ok{};
};

VulkanBits vulkanBits(QRhi& rhi) noexcept
{
  VulkanBits b;
  if(rhi.backend() != QRhi::Vulkan)
    return b;
  const auto* nh = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
  if(!nh || !nh->dev || !nh->physDev)
    return b;
  // QRhiVulkanNativeHandles only carries the QVulkanInstance from Qt 6.6 on;
  // score creates every Vulkan RenderState against the process-wide one, so
  // take it from there and stay buildable on older Qt.
  auto* inst = score::gfx::staticVulkanInstance(false);
  if(!inst)
    return b;
  b.dev = nh->dev;
  b.phys = nh->physDev;
  b.inst = inst;
  b.ok = true;
  return b;
}
} // namespace

std::size_t VkHostImportUpload::requiredAlignment(QRhi& rhi) noexcept
{
  // Falling back silently is how two separate mistakes stayed invisible during
  // bring-up (extension requested on one platform only; a stale binary), so say
  // which check failed. qWarning, not qDebug: some targets strip debug output.
  const bool vk = rhi.backend() == QRhi::Vulkan;
  const auto b = vulkanBits(rhi);
  if(!b.ok)
  {
    if(vk)
      qWarning() << "VkHostImportUpload: no Vulkan native handles / instance";
    return 0;
  }

  // The entry point only resolves when the device was created with
  // VK_EXT_external_memory_host (see ScreenNode's deviceExtensions list).
  if(!b.inst->getInstanceProcAddr("vkGetMemoryHostPointerPropertiesEXT"))
  {
    qWarning() << "VkHostImportUpload: vkGetMemoryHostPointerPropertiesEXT "
                  "unresolved - VK_EXT_external_memory_host not enabled";
    return 0;
  }
  auto props2 = (PFN_vkGetPhysicalDeviceProperties2)b.inst->getInstanceProcAddr(
      "vkGetPhysicalDeviceProperties2");
  if(!props2)
  {
    qWarning() << "VkHostImportUpload: vkGetPhysicalDeviceProperties2 unresolved";
    return 0;
  }

  VkPhysicalDeviceExternalMemoryHostPropertiesEXT hp{};
  hp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT;
  VkPhysicalDeviceProperties2 p2{};
  p2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  p2.pNext = &hp;
  props2(b.phys, &p2);
  if(hp.minImportedHostPointerAlignment == 0)
  {
    qWarning() << "VkHostImportUpload: driver reports zero import alignment";
    return 0;
  }
  return std::size_t(hp.minImportedHostPointerAlignment);
}

bool importHostPointerBuffer(
    QRhi& rhi, void* host, std::size_t bytes, unsigned bufferUsage,
    bool requireHostCoherent, VkHostImportedBuffer& out)
{
  out = {};
  const std::size_t align = VkHostImportUpload::requiredAlignment(rhi);
  if(align == 0 || !host || bytes == 0)
    return false;
  if((reinterpret_cast<std::uintptr_t>(host) % align) != 0)
  {
    qWarning() << "importHostPointerBuffer: pointer is not" << align << "aligned";
    return false;
  }

  const auto b = vulkanBits(rhi);
  if(!b.ok)
    return false;
  auto* df = b.inst->deviceFunctions(b.dev);
  auto getHostPtrProps = (PFN_vkGetMemoryHostPointerPropertiesEXT)
      b.inst->getInstanceProcAddr("vkGetMemoryHostPointerPropertiesEXT");
  if(!df || !getHostPtrProps)
    return false;

  const std::size_t importedBytes = ((bytes + align - 1) / align) * align;

  VkMemoryHostPointerPropertiesEXT hpp{};
  hpp.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT;
  if(getHostPtrProps(
         b.dev, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, host, &hpp)
         != VK_SUCCESS
     || hpp.memoryTypeBits == 0)
    return false;

  VkExternalMemoryBufferCreateInfo ebi{};
  ebi.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
  ebi.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
  VkBufferCreateInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bi.pNext = &ebi;
  bi.size = importedBytes;
  bi.usage = VkBufferUsageFlags(bufferUsage);
  VkBuffer buf{};
  if(df->vkCreateBuffer(b.dev, &bi, nullptr, &buf) != VK_SUCCESS)
    return false;

  VkPhysicalDeviceMemoryProperties mp{};
  b.inst->functions()->vkGetPhysicalDeviceMemoryProperties(b.phys, &mp);
  VkMemoryRequirements mr{};
  df->vkGetBufferMemoryRequirements(b.dev, buf, &mr);

  const VkMemoryPropertyFlags coherent = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  const VkMemoryPropertyFlags passes[] = {
      requireHostCoherent ? (coherent | VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
                          : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
      requireHostCoherent ? coherent : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
  };
  int typeIdx = -1;
  for(VkMemoryPropertyFlags wanted : passes)
  {
    for(uint32_t i = 0; typeIdx < 0 && i < mp.memoryTypeCount; ++i)
    {
      if(!((mr.memoryTypeBits & hpp.memoryTypeBits) & (1u << i)))
        continue;
      if((mp.memoryTypes[i].propertyFlags & wanted) == wanted)
        typeIdx = int(i);
    }
    if(typeIdx >= 0)
      break;
  }
  if(typeIdx < 0)
  {
    df->vkDestroyBuffer(b.dev, buf, nullptr);
    return false;
  }

  VkImportMemoryHostPointerInfoEXT imp{};
  imp.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT;
  imp.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
  imp.pHostPointer = host;
  VkMemoryAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.pNext = &imp;
  ai.allocationSize = importedBytes;
  ai.memoryTypeIndex = uint32_t(typeIdx);
  VkDeviceMemory mem{};
  if(df->vkAllocateMemory(b.dev, &ai, nullptr, &mem) != VK_SUCCESS)
  {
    df->vkDestroyBuffer(b.dev, buf, nullptr);
    return false;
  }
  if(df->vkBindBufferMemory(b.dev, buf, mem, 0) != VK_SUCCESS)
  {
    df->vkFreeMemory(b.dev, mem, nullptr);
    df->vkDestroyBuffer(b.dev, buf, nullptr);
    return false;
  }

  out.buffer = reinterpret_cast<void*>(buf);
  out.memory = reinterpret_cast<void*>(mem);
  out.importedBytes = importedBytes;
  return true;
}

void releaseHostImportedBuffer(QRhi& rhi, VkHostImportedBuffer& buf)
{
  const auto b = vulkanBits(rhi);
  if(b.ok)
  {
    if(auto* df = b.inst->deviceFunctions(b.dev))
    {
      if(buf.buffer)
        df->vkDestroyBuffer(b.dev, reinterpret_cast<VkBuffer>(buf.buffer), nullptr);
      if(buf.memory)
        df->vkFreeMemory(b.dev, reinterpret_cast<VkDeviceMemory>(buf.memory), nullptr);
    }
  }
  buf = {};
}

VkHostImportUpload::~VkHostImportUpload()
{
  release();
}

bool VkHostImportUpload::init(
    QRhi& rhi, const std::vector<void*>& slots, std::size_t bytes)
{
  release();
  const std::size_t align = requiredAlignment(rhi);
  if(align == 0 || slots.empty() || bytes == 0)
    return false;

  const auto b = vulkanBits(rhi);
  if(!b.ok)
    return false;

  auto* d = new Impl;
  d->rhi = &rhi;
  d->dev = b.dev;
  d->phys = b.phys;
  d->inst = b.inst;
  d->df = b.inst->deviceFunctions(b.dev);
  d->getHostPtrProps = (PFN_vkGetMemoryHostPointerPropertiesEXT)
      b.inst->getInstanceProcAddr("vkGetMemoryHostPointerPropertiesEXT");
  d->importedBytes = ((bytes + align - 1) / align) * align;
  if(!d->df || !d->getHostPtrProps)
  {
    delete d;
    return false;
  }

  for(void* host : slots)
  {
    VkHostImportedBuffer ib;
    if(importHostPointerBuffer(
           rhi, host, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, false, ib))
    {
      d->mems.push_back(reinterpret_cast<VkDeviceMemory>(ib.memory));
      m_buffers.push_back(ib.buffer);
      continue;
    }
    for(auto* raw : m_buffers)
      d->df->vkDestroyBuffer(d->dev, reinterpret_cast<VkBuffer>(raw), nullptr);
    for(auto mem : d->mems)
      d->df->vkFreeMemory(d->dev, mem, nullptr);
    m_buffers.clear();
    delete d;
    return false;
  }

  m_d = d;
  return true;
}

void VkHostImportUpload::release()
{
  if(!m_d)
  {
    m_buffers.clear();
    return;
  }
  for(auto* raw : m_buffers)
    m_d->df->vkDestroyBuffer(m_d->dev, reinterpret_cast<VkBuffer>(raw), nullptr);
  for(auto mem : m_d->mems)
    m_d->df->vkFreeMemory(m_d->dev, mem, nullptr);
  m_buffers.clear();
  delete m_d;
  m_d = nullptr;
}

bool VkHostImportUpload::copyToTexture(
    QRhiCommandBuffer& cb, QRhiTexture& tex, std::size_t slot, int width,
    int height) noexcept
{
  if(!m_d || slot >= m_buffers.size() || width <= 0 || height <= 0)
    return false;
  auto nt = tex.nativeTexture();
  if(!nt.object)
    return false;
  const VkImage img = VkImage(nt.object);
  const VkBuffer buf = reinterpret_cast<VkBuffer>(m_buffers[slot]);

  // beginExternal flushes QRhi's own pending work and hands us the live VkCommandBuffer.
  cb.beginExternal();
  const auto* nh
      = static_cast<const QRhiVulkanCommandBufferNativeHandles*>(cb.nativeHandles());
  if(!nh || !nh->commandBuffer)
  {
    cb.endExternal();
    return false;
  }

  auto transition = [&](VkImageLayout from, VkImageLayout to) {
    VkImageMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    b.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    m_d->df->vkCmdPipelineBarrier(
        nh->commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
  };

  transition(
      VkImageLayout(nt.layout ? nt.layout : VK_IMAGE_LAYOUT_UNDEFINED),
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkBufferImageCopy region{};
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageExtent = {uint32_t(width), uint32_t(height), 1};
  m_d->df->vkCmdCopyBufferToImage(
      nh->commandBuffer, buf, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  transition(
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  cb.endExternal();

  // QRhi tracks layouts itself; without this its next barrier would use a stale
  // oldLayout and the driver would reject the transition.
  tex.setNativeLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  return true;
}

#else // no Vulkan

struct VkHostImportUpload::Impl
{
};
bool importHostPointerBuffer(
    QRhi&, void*, std::size_t, unsigned, bool, VkHostImportedBuffer& out)
{
  out = {};
  return false;
}
void releaseHostImportedBuffer(QRhi&, VkHostImportedBuffer& buf)
{
  buf = {};
}
std::size_t VkHostImportUpload::requiredAlignment(QRhi&) noexcept
{
  return 0;
}
VkHostImportUpload::~VkHostImportUpload() = default;
bool VkHostImportUpload::init(QRhi&, const std::vector<void*>&, std::size_t)
{
  return false;
}
void VkHostImportUpload::release() { }
bool VkHostImportUpload::copyToTexture(
    QRhiCommandBuffer&, QRhiTexture&, std::size_t, int, int) noexcept
{
  return false;
}

#endif

} // namespace score::gfx::interop

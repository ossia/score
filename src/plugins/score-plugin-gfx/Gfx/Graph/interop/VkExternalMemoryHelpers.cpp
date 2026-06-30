#include <Gfx/Graph/interop/VkExternalMemoryHelpers.hpp>

#if QT_HAS_VULKAN

#include <QVulkanDeviceFunctions>
#include <QVulkanFunctions>
#include <QVulkanInstance>
#include <QDebug>

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

// Resolve the per-device dispatch table. All core vkXxx calls in this
// file go through this — never use free vkXxx symbols, because
// score-plugin-gfx does not link against libvulkan directly.
QVulkanDeviceFunctions* devFuncs(const VulkanCtx& v)
{
  if(!v.qInst || !v.dev)
    return nullptr;
  return v.qInst->deviceFunctions(v.dev);
}

QVulkanFunctions* instFuncs(const VulkanCtx& v)
{
  if(!v.qInst)
    return nullptr;
  return v.qInst->functions();
}

PFN_vkVoidFunction resolveDeviceFn(const VulkanCtx& v, const char* name)
{
  if(!v.qInst || !v.dev)
    return nullptr;
  // Gotcha documented at SpoutInput.cpp:612-625 — extension functions
  // must be resolved via vkGetDeviceProcAddr, not vkGetInstanceProcAddr,
  // or some drivers crash. We bootstrap vkGetDeviceProcAddr via the
  // instance loader.
  auto getDeviceProcAddr = (PFN_vkGetDeviceProcAddr)v.qInst->getInstanceProcAddr(
      "vkGetDeviceProcAddr");
  if(!getDeviceProcAddr)
    return nullptr;
  return getDeviceProcAddr(v.dev, name);
}

// Returns the index of a memory type satisfying:
//   - typeBits (from vk*MemoryRequirements)
//   - propsRequired (e.g. DEVICE_LOCAL_BIT)
// Falls back to any compatible type if `allowFallback` is true.
std::optional<uint32_t> findMemoryType(
    const VulkanCtx& v, uint32_t typeBits,
    VkMemoryPropertyFlags propsRequired, bool allowFallback)
{
  auto vf = instFuncs(v);
  if(!vf)
    return std::nullopt;

  VkPhysicalDeviceMemoryProperties memProps{};
  vf->vkGetPhysicalDeviceMemoryProperties(v.physDev, &memProps);

  for(uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
  {
    if(!(typeBits & (1u << i)))
      continue;
    if((memProps.memoryTypes[i].propertyFlags & propsRequired) == propsRequired)
      return i;
  }
  if(!allowFallback)
    return std::nullopt;
  for(uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
  {
    if(typeBits & (1u << i))
      return i;
  }
  return std::nullopt;
}

// Intersect typeBits from image/buffer memory requirements with the
// handle-specific allowed types, per Vulkan spec for external memory import.
std::optional<uint32_t> intersectImportTypeBits(
    const VulkanCtx& v, const ExternalHandle& h, uint32_t typeBitsFromReqs)
{
  uint32_t externalAllowed = 0xFFFFFFFFu; // default: don't restrict

#if defined(_WIN32)
  auto fp = (PFN_vkGetMemoryWin32HandlePropertiesKHR)resolveDeviceFn(
      v, "vkGetMemoryWin32HandlePropertiesKHR");
  if(fp)
  {
    VkMemoryWin32HandlePropertiesKHR props{};
    props.sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR;
    if(fp(v.dev, h.type, h.handle, &props) == VK_SUCCESS)
      externalAllowed = props.memoryTypeBits;
  }
#else
  auto fp = (PFN_vkGetMemoryFdPropertiesKHR)resolveDeviceFn(
      v, "vkGetMemoryFdPropertiesKHR");
  if(fp)
  {
    VkMemoryFdPropertiesKHR props{};
    props.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
    if(fp(v.dev, h.type, h.fd, &props) == VK_SUCCESS)
      externalAllowed = props.memoryTypeBits;
  }
#endif

  const uint32_t intersected = typeBitsFromReqs & externalAllowed;
  return findMemoryType(
      v, intersected, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      /*allowFallback=*/true);
}

bool probeImageFormatSupported(
    const VulkanCtx& v, const ExternalImageDesc& desc, bool forImport)
{
  auto vf = instFuncs(v);
  if(!vf)
    return false;

  VkPhysicalDeviceExternalImageFormatInfo extInfo{};
  extInfo.sType
      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
  extInfo.handleType = desc.handleType;

  VkPhysicalDeviceImageFormatInfo2 imgInfo{};
  imgInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
  imgInfo.pNext = &extInfo;
  imgInfo.format = desc.format;
  imgInfo.type = VK_IMAGE_TYPE_2D;
  imgInfo.tiling = desc.tiling;
  imgInfo.usage = desc.usage;
  imgInfo.flags = 0;

  VkExternalImageFormatProperties extProps{};
  extProps.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;

  VkImageFormatProperties2 props{};
  props.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
  props.pNext = &extProps;

  if(vf->vkGetPhysicalDeviceImageFormatProperties2(
         v.physDev, &imgInfo, &props)
     != VK_SUCCESS)
  {
    return false;
  }
  const auto feat
      = extProps.externalMemoryProperties.externalMemoryFeatures;
  const VkExternalMemoryFeatureFlags needed
      = forImport ? VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT
                  : VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT;
  return (feat & needed) != 0;
}

} // namespace

// =============================================================================
// EXPORT
// =============================================================================

std::optional<ExternalImage>
createExportableImage(const VulkanCtx& v, const ExternalImageDesc& desc)
{
  auto df = devFuncs(v);
  if(!df || !v.physDev)
    return std::nullopt;
  if(!probeImageFormatSupported(v, desc, /*forImport=*/false))
  {
    qWarning() << "createExportableImage: format/usage/handleType not supported"
               << "format" << desc.format << "handleType" << desc.handleType;
    return std::nullopt;
  }

  VkExternalMemoryImageCreateInfo extImgInfo{};
  extImgInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
  extImgInfo.handleTypes = desc.handleType;

  VkImageCreateInfo imgInfo{};
  imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imgInfo.pNext = &extImgInfo;
  imgInfo.imageType = VK_IMAGE_TYPE_2D;
  imgInfo.format = desc.format;
  imgInfo.extent = desc.extent;
  imgInfo.mipLevels = 1;
  imgInfo.arrayLayers = 1;
  imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imgInfo.tiling = desc.tiling;
  imgInfo.usage = desc.usage;
  imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  ExternalImage out{};
  if(df->vkCreateImage(v.dev, &imgInfo, nullptr, &out.image) != VK_SUCCESS)
  {
    qWarning() << "createExportableImage: vkCreateImage failed";
    return std::nullopt;
  }

  VkMemoryRequirements memReq{};
  df->vkGetImageMemoryRequirements(v.dev, out.image, &memReq);
  out.size = memReq.size;

  const VkMemoryPropertyFlags props
      = desc.preferDeviceLocal ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0;
  auto memType = findMemoryType(
      v, memReq.memoryTypeBits, props,
      /*allowFallback=*/desc.preferDeviceLocal);
  if(!memType)
  {
    qWarning() << "createExportableImage: no compatible memory type";
    df->vkDestroyImage(v.dev, out.image, nullptr);
    return std::nullopt;
  }

  VkExportMemoryAllocateInfo exportInfo{};
  exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
  exportInfo.handleTypes = desc.handleType;

  VkMemoryDedicatedAllocateInfo dedicatedInfo{};
  dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
  dedicatedInfo.image = out.image;
  if(desc.dedicated)
    exportInfo.pNext = &dedicatedInfo;

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.pNext = &exportInfo;
  allocInfo.allocationSize = memReq.size;
  allocInfo.memoryTypeIndex = *memType;

  if(df->vkAllocateMemory(v.dev, &allocInfo, nullptr, &out.memory)
     != VK_SUCCESS)
  {
    qWarning() << "createExportableImage: vkAllocateMemory failed";
    df->vkDestroyImage(v.dev, out.image, nullptr);
    return std::nullopt;
  }

  if(df->vkBindImageMemory(v.dev, out.image, out.memory, 0) != VK_SUCCESS)
  {
    qWarning() << "createExportableImage: vkBindImageMemory failed";
    destroyExternal(v, out);
    return std::nullopt;
  }
  return out;
}

std::optional<ExternalBuffer>
createExportableBuffer(const VulkanCtx& v, const ExternalBufferDesc& desc)
{
  auto df = devFuncs(v);
  if(!df || !v.physDev || desc.size == 0)
    return std::nullopt;

  VkExternalMemoryBufferCreateInfo extBufInfo{};
  extBufInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
  extBufInfo.handleTypes = desc.handleType;

  VkBufferCreateInfo bufInfo{};
  bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufInfo.pNext = &extBufInfo;
  bufInfo.size = desc.size;
  bufInfo.usage = desc.usage;
  bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  ExternalBuffer out{};
  if(df->vkCreateBuffer(v.dev, &bufInfo, nullptr, &out.buffer) != VK_SUCCESS)
  {
    qWarning() << "createExportableBuffer: vkCreateBuffer failed";
    return std::nullopt;
  }

  VkMemoryRequirements memReq{};
  df->vkGetBufferMemoryRequirements(v.dev, out.buffer, &memReq);
  out.size = memReq.size;

  auto memType = findMemoryType(
      v, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      /*allowFallback=*/true);
  if(!memType)
  {
    qWarning() << "createExportableBuffer: no compatible memory type";
    df->vkDestroyBuffer(v.dev, out.buffer, nullptr);
    return std::nullopt;
  }

  VkExportMemoryAllocateInfo exportInfo{};
  exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
  exportInfo.handleTypes = desc.handleType;

  VkMemoryDedicatedAllocateInfo dedicatedInfo{};
  dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
  dedicatedInfo.buffer = out.buffer;
  if(desc.dedicated)
    exportInfo.pNext = &dedicatedInfo;

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.pNext = &exportInfo;
  allocInfo.allocationSize = memReq.size;
  allocInfo.memoryTypeIndex = *memType;

  if(df->vkAllocateMemory(v.dev, &allocInfo, nullptr, &out.memory)
     != VK_SUCCESS)
  {
    qWarning() << "createExportableBuffer: vkAllocateMemory failed";
    df->vkDestroyBuffer(v.dev, out.buffer, nullptr);
    return std::nullopt;
  }

  if(df->vkBindBufferMemory(v.dev, out.buffer, out.memory, 0) != VK_SUCCESS)
  {
    qWarning() << "createExportableBuffer: vkBindBufferMemory failed";
    destroyExternal(v, out);
    return std::nullopt;
  }
  return out;
}

std::optional<ExternalHandle> exportMemoryHandle(
    const VulkanCtx& v, VkDeviceMemory mem,
    VkExternalMemoryHandleTypeFlagBits type)
{
  if(!v.dev || !mem)
    return std::nullopt;

  ExternalHandle h{};
  h.type = type;

#if defined(_WIN32)
  auto fp = (PFN_vkGetMemoryWin32HandleKHR)resolveDeviceFn(
      v, "vkGetMemoryWin32HandleKHR");
  if(!fp)
  {
    qWarning() << "exportMemoryHandle: vkGetMemoryWin32HandleKHR unavailable";
    return std::nullopt;
  }
  VkMemoryGetWin32HandleInfoKHR info{};
  info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
  info.memory = mem;
  info.handleType = type;
  if(fp(v.dev, &info, &h.handle) != VK_SUCCESS || !h.handle)
  {
    qWarning() << "exportMemoryHandle: vkGetMemoryWin32HandleKHR failed";
    return std::nullopt;
  }
#else
  auto fp = (PFN_vkGetMemoryFdKHR)resolveDeviceFn(v, "vkGetMemoryFdKHR");
  if(!fp)
  {
    qWarning() << "exportMemoryHandle: vkGetMemoryFdKHR unavailable";
    return std::nullopt;
  }
  VkMemoryGetFdInfoKHR info{};
  info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
  info.memory = mem;
  info.handleType = type;
  if(fp(v.dev, &info, &h.fd) != VK_SUCCESS || h.fd < 0)
  {
    qWarning() << "exportMemoryHandle: vkGetMemoryFdKHR failed";
    return std::nullopt;
  }
#endif
  return h;
}

// =============================================================================
// RDMA (VK_NV_external_memory_rdma)
// =============================================================================

#ifdef VK_NV_external_memory_rdma

std::optional<std::uint64_t>
getMemoryRemoteAddress(const VulkanCtx& v, VkDeviceMemory mem)
{
  if(!v.dev || !mem)
    return std::nullopt;
  auto fp = (PFN_vkGetMemoryRemoteAddressNV)resolveDeviceFn(
      v, "vkGetMemoryRemoteAddressNV");
  if(!fp)
  {
    qWarning() << "getMemoryRemoteAddress: vkGetMemoryRemoteAddressNV unavailable";
    return std::nullopt;
  }
  VkMemoryGetRemoteAddressInfoNV info{};
  info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_REMOTE_ADDRESS_INFO_NV;
  info.memory = mem;
  info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_RDMA_ADDRESS_BIT_NV;
  VkRemoteAddressNV addr{};
  if(fp(v.dev, &info, &addr) != VK_SUCCESS)
  {
    qWarning() << "getMemoryRemoteAddress: vkGetMemoryRemoteAddressNV failed";
    return std::nullopt;
  }
  return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(addr));
}

std::optional<RdmaBuffer> createRdmaBuffer(const VulkanCtx& v, VkDeviceSize size)
{
  auto df = devFuncs(v);
  if(!df || !v.physDev || size == 0)
    return std::nullopt;

  VkExternalMemoryBufferCreateInfo extBufInfo{};
  extBufInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
  extBufInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_RDMA_ADDRESS_BIT_NV;

  VkBufferCreateInfo bufInfo{};
  bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufInfo.pNext = &extBufInfo;
  bufInfo.size = size;
  bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                  | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                  | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  RdmaBuffer out{};
  if(df->vkCreateBuffer(v.dev, &bufInfo, nullptr, &out.buffer) != VK_SUCCESS)
  {
    qWarning() << "createRdmaBuffer: vkCreateBuffer failed";
    return std::nullopt;
  }

  VkMemoryRequirements memReq{};
  df->vkGetBufferMemoryRequirements(v.dev, out.buffer, &memReq);
  out.size = memReq.size;

  // RDMA-capable memory type only — no fallback (a non-RDMA type would not be
  // remotely addressable by the card).
  auto memType = findMemoryType(
      v, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV,
      /*allowFallback=*/false);
  if(!memType)
  {
    qWarning() << "createRdmaBuffer: no RDMA-capable memory type";
    df->vkDestroyBuffer(v.dev, out.buffer, nullptr);
    return std::nullopt;
  }

  VkExportMemoryAllocateInfo exportInfo{};
  exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
  exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_RDMA_ADDRESS_BIT_NV;

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.pNext = &exportInfo;
  allocInfo.allocationSize = memReq.size;
  allocInfo.memoryTypeIndex = *memType;

  if(df->vkAllocateMemory(v.dev, &allocInfo, nullptr, &out.memory) != VK_SUCCESS)
  {
    qWarning() << "createRdmaBuffer: vkAllocateMemory failed";
    df->vkDestroyBuffer(v.dev, out.buffer, nullptr);
    return std::nullopt;
  }
  if(df->vkBindBufferMemory(v.dev, out.buffer, out.memory, 0) != VK_SUCCESS)
  {
    qWarning() << "createRdmaBuffer: vkBindBufferMemory failed";
    destroyRdma(v, out);
    return std::nullopt;
  }

  auto addr = getMemoryRemoteAddress(v, out.memory);
  if(!addr)
  {
    destroyRdma(v, out);
    return std::nullopt;
  }
  out.remoteAddress = *addr;
  return out;
}

void destroyRdma(const VulkanCtx& v, RdmaBuffer& b)
{
  auto df = devFuncs(v);
  if(df)
  {
    if(b.buffer)
      df->vkDestroyBuffer(v.dev, b.buffer, nullptr);
    if(b.memory)
      df->vkFreeMemory(v.dev, b.memory, nullptr);
  }
  b = {};
}

#else // VK_NV_external_memory_rdma not in these Vulkan headers

std::optional<RdmaBuffer> createRdmaBuffer(const VulkanCtx&, VkDeviceSize)
{
  return std::nullopt;
}
std::optional<std::uint64_t> getMemoryRemoteAddress(const VulkanCtx&, VkDeviceMemory)
{
  return std::nullopt;
}
void destroyRdma(const VulkanCtx&, RdmaBuffer&) { }

#endif // VK_NV_external_memory_rdma

// =============================================================================
// IMPORT
// =============================================================================

namespace
{

struct ImportChain
{
#if defined(_WIN32)
  VkImportMemoryWin32HandleInfoKHR win32{};
#else
  VkImportMemoryFdInfoKHR fd{};
#endif
  VkMemoryDedicatedAllocateInfo dedicated{};
};

void* buildImportChain(
    ImportChain& chain, const ExternalHandle& h, bool wantDedicated,
    VkImage dedicatedImage, VkBuffer dedicatedBuffer)
{
  void* head = nullptr;
#if defined(_WIN32)
  chain.win32.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
  chain.win32.handleType = h.type;
  chain.win32.handle = h.handle;
  head = &chain.win32;
#else
  chain.fd.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
  chain.fd.handleType = h.type;
  chain.fd.fd = h.fd;
  head = &chain.fd;
#endif
  if(wantDedicated)
  {
    chain.dedicated.sType
        = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    chain.dedicated.image = dedicatedImage;
    chain.dedicated.buffer = dedicatedBuffer;
#if defined(_WIN32)
    chain.win32.pNext = &chain.dedicated;
#else
    chain.fd.pNext = &chain.dedicated;
#endif
  }
  return head;
}

} // namespace

std::optional<ExternalImage> importExternalImage(
    const VulkanCtx& v, const ExternalImageDesc& desc, const ExternalHandle& h)
{
  auto df = devFuncs(v);
  if(!df || !v.physDev || !h.isValid())
    return std::nullopt;
  if(h.type != desc.handleType)
  {
    qWarning() << "importExternalImage: handle type mismatch"
               << "desc" << desc.handleType << "handle" << h.type;
    return std::nullopt;
  }
  if(!probeImageFormatSupported(v, desc, /*forImport=*/true))
  {
    qWarning() << "importExternalImage: format/usage/handleType not importable";
    return std::nullopt;
  }

  VkExternalMemoryImageCreateInfo extImgInfo{};
  extImgInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
  extImgInfo.handleTypes = desc.handleType;

  VkImageCreateInfo imgInfo{};
  imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imgInfo.pNext = &extImgInfo;
  imgInfo.imageType = VK_IMAGE_TYPE_2D;
  imgInfo.format = desc.format;
  imgInfo.extent = desc.extent;
  imgInfo.mipLevels = 1;
  imgInfo.arrayLayers = 1;
  imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imgInfo.tiling = desc.tiling;
  imgInfo.usage = desc.usage;
  imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  ExternalImage out{};
  if(df->vkCreateImage(v.dev, &imgInfo, nullptr, &out.image) != VK_SUCCESS)
  {
    qWarning() << "importExternalImage: vkCreateImage failed";
    return std::nullopt;
  }

  VkMemoryRequirements memReq{};
  df->vkGetImageMemoryRequirements(v.dev, out.image, &memReq);
  out.size = memReq.size;

  auto memType = intersectImportTypeBits(v, h, memReq.memoryTypeBits);
  if(!memType)
  {
    qWarning() << "importExternalImage: no compatible memory type";
    df->vkDestroyImage(v.dev, out.image, nullptr);
    return std::nullopt;
  }

  ImportChain chain;
  void* pNext = buildImportChain(
      chain, h, desc.dedicated, out.image, VK_NULL_HANDLE);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.pNext = pNext;
  allocInfo.allocationSize = memReq.size;
  allocInfo.memoryTypeIndex = *memType;

  if(df->vkAllocateMemory(v.dev, &allocInfo, nullptr, &out.memory)
     != VK_SUCCESS)
  {
    qWarning() << "importExternalImage: vkAllocateMemory failed";
    df->vkDestroyImage(v.dev, out.image, nullptr);
    return std::nullopt;
  }
  if(df->vkBindImageMemory(v.dev, out.image, out.memory, 0) != VK_SUCCESS)
  {
    qWarning() << "importExternalImage: vkBindImageMemory failed";
    destroyExternal(v, out);
    return std::nullopt;
  }
  return out;
}

std::optional<ExternalBuffer> importExternalBuffer(
    const VulkanCtx& v, const ExternalBufferDesc& desc, const ExternalHandle& h)
{
  auto df = devFuncs(v);
  if(!df || !v.physDev || !h.isValid() || desc.size == 0)
    return std::nullopt;
  if(h.type != desc.handleType)
  {
    qWarning() << "importExternalBuffer: handle type mismatch";
    return std::nullopt;
  }

  VkExternalMemoryBufferCreateInfo extBufInfo{};
  extBufInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
  extBufInfo.handleTypes = desc.handleType;

  VkBufferCreateInfo bufInfo{};
  bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufInfo.pNext = &extBufInfo;
  bufInfo.size = desc.size;
  bufInfo.usage = desc.usage;
  bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  ExternalBuffer out{};
  if(df->vkCreateBuffer(v.dev, &bufInfo, nullptr, &out.buffer) != VK_SUCCESS)
  {
    qWarning() << "importExternalBuffer: vkCreateBuffer failed";
    return std::nullopt;
  }

  VkMemoryRequirements memReq{};
  df->vkGetBufferMemoryRequirements(v.dev, out.buffer, &memReq);
  out.size = memReq.size;

  auto memType = intersectImportTypeBits(v, h, memReq.memoryTypeBits);
  if(!memType)
  {
    qWarning() << "importExternalBuffer: no compatible memory type";
    df->vkDestroyBuffer(v.dev, out.buffer, nullptr);
    return std::nullopt;
  }

  ImportChain chain;
  void* pNext = buildImportChain(
      chain, h, desc.dedicated, VK_NULL_HANDLE, out.buffer);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.pNext = pNext;
  allocInfo.allocationSize = memReq.size;
  allocInfo.memoryTypeIndex = *memType;

  if(df->vkAllocateMemory(v.dev, &allocInfo, nullptr, &out.memory)
     != VK_SUCCESS)
  {
    qWarning() << "importExternalBuffer: vkAllocateMemory failed";
    df->vkDestroyBuffer(v.dev, out.buffer, nullptr);
    return std::nullopt;
  }
  if(df->vkBindBufferMemory(v.dev, out.buffer, out.memory, 0) != VK_SUCCESS)
  {
    qWarning() << "importExternalBuffer: vkBindBufferMemory failed";
    destroyExternal(v, out);
    return std::nullopt;
  }
  return out;
}

// =============================================================================
// Cleanup
// =============================================================================

void destroyExternal(const VulkanCtx& v, ExternalImage& img)
{
  auto df = devFuncs(v);
  if(!df)
    return;
  if(img.image)
    df->vkDestroyImage(v.dev, img.image, nullptr);
  if(img.memory)
    df->vkFreeMemory(v.dev, img.memory, nullptr);
  img = {};
}

void destroyExternal(const VulkanCtx& v, ExternalBuffer& buf)
{
  auto df = devFuncs(v);
  if(!df)
    return;
  if(buf.buffer)
    df->vkDestroyBuffer(v.dev, buf.buffer, nullptr);
  if(buf.memory)
    df->vkFreeMemory(v.dev, buf.memory, nullptr);
  buf = {};
}

// =============================================================================
// DMA-BUF modifier capability probes
// =============================================================================

#if defined(__linux__) && defined(VK_EXT_image_drm_format_modifier)

bool can_import_dmabuf_modifier(
    const VulkanCtx& v, VkFormat format, std::uint64_t drm_modifier,
    std::uint32_t width, std::uint32_t height) noexcept
{
  if(!v.physDev || !v.qInst)
    return false;
  auto* vf = v.qInst->functions();
  if(!vf)
    return false;

  // VkPhysicalDeviceImageFormatInfo2 chain:
  //   info → ExternalImage (DMA_BUF) → DrmFormatModifier(modifier)
  // Returns supported iff the (format, modifier) combination is
  // importable for the requested usage.

  VkPhysicalDeviceImageDrmFormatModifierInfoEXT modInfo{};
  modInfo.sType
      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT;
  modInfo.drmFormatModifier = drm_modifier;
  modInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  modInfo.queueFamilyIndexCount = 0;
  modInfo.pQueueFamilyIndices = nullptr;

  VkPhysicalDeviceExternalImageFormatInfo extInfo{};
  extInfo.sType
      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
  extInfo.pNext = &modInfo;
  extInfo.handleType
      = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

  VkPhysicalDeviceImageFormatInfo2 imgInfo{};
  imgInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
  imgInfo.pNext = &extInfo;
  imgInfo.format = format;
  imgInfo.type = VK_IMAGE_TYPE_2D;
  imgInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
  imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT
                  | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                  | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  imgInfo.flags = 0;

  VkExternalImageFormatProperties extProps{};
  extProps.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;

  VkImageFormatProperties2 props{};
  props.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
  props.pNext = &extProps;

  if(vf->vkGetPhysicalDeviceImageFormatProperties2(
         v.physDev, &imgInfo, &props)
     != VK_SUCCESS)
    return false;

  if((extProps.externalMemoryProperties.externalMemoryFeatures
      & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)
     == 0)
    return false;

  // Extent must fit the per-modifier maximum the driver reports.
  const auto& maxExt = props.imageFormatProperties.maxExtent;
  return width <= maxExt.width && height <= maxExt.height;
}

std::vector<std::uint64_t> supported_dmabuf_modifiers(
    const VulkanCtx& v, VkFormat format) noexcept
{
  std::vector<std::uint64_t> result;
  if(!v.physDev || !v.qInst)
  {
    result.push_back(0); // LINEAR fallback
    return result;
  }
  auto* vf = v.qInst->functions();
  if(!vf)
  {
    result.push_back(0);
    return result;
  }

  // Query modifier count first via the modifier properties list.
  VkDrmFormatModifierPropertiesListEXT modList{};
  modList.sType
      = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;

  VkFormatProperties2 fp{};
  fp.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
  fp.pNext = &modList;

  vf->vkGetPhysicalDeviceFormatProperties2(v.physDev, format, &fp);
  if(modList.drmFormatModifierCount == 0)
  {
    result.push_back(0);
    return result;
  }

  std::vector<VkDrmFormatModifierPropertiesEXT> mods(
      modList.drmFormatModifierCount);
  modList.pDrmFormatModifierProperties = mods.data();
  vf->vkGetPhysicalDeviceFormatProperties2(v.physDev, format, &fp);

  result.reserve(mods.size() + 1);
  bool has_linear = false;
  for(const auto& m : mods)
  {
    result.push_back(m.drmFormatModifier);
    if(m.drmFormatModifier == 0)
      has_linear = true;
  }
  // Always advertise LINEAR even if the driver doesn't enumerate it
  // — most consumers expect it as a safe fallback.
  if(!has_linear)
    result.push_back(0);
  return result;
}

#else

bool can_import_dmabuf_modifier(
    const VulkanCtx&, VkFormat, std::uint64_t modifier, std::uint32_t,
    std::uint32_t) noexcept
{
  // No VK_EXT_image_drm_format_modifier — only LINEAR is safe to
  // assume; let the caller fall back to SHM otherwise.
  return modifier == 0;
}

std::vector<std::uint64_t>
supported_dmabuf_modifiers(const VulkanCtx&, VkFormat) noexcept
{
  return {0}; // LINEAR only
}

#endif

} // namespace score::gfx::vkinterop

#endif // QT_HAS_VULKAN

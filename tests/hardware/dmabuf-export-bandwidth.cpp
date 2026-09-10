// How fast can a driver write into an image that is exported as a dma-buf?
//
// score's PipeWire video output renders a frame and copies it into an exported
// image so a consumer can import the file descriptor. At 7680x4320 that copy
// costs orders of magnitude more than the same copy into an ordinary texture.
// Everything else was ruled out first: the scene render is cheap, and the
// pipewire handover is negligible.
//
// This is the smallest thing that reproduces it: no score, no pipewire, no Qt.
// One source image, one destination, vkCmdCopyImage between them, timed. The
// destination is built five different ways and the table says which of them is
// slow.
//
// The suspect is the tiling. VK_EXT_image_drm_format_modifier exists precisely
// because an exported image needs a layout the importer can describe, and the
// Vulkan documentation is blunt about the alternative: LINEAR is "universally
// supported" and "typically offers poor performance", while OPTIMAL "cannot be
// directly shared via dma-buf" because nobody outside the driver knows the
// layout. The extension's whole point is to get a vendor-tiled layout that IS
// shareable, by name. score's export asks for LINEAR.
//
// The table has one row per destination. "copy" is a full-frame transfer into
// it, which is what score's render-then-copy path does; "clear" is the graphics
// engine writing every pixel of it, which is the shape of rendering INTO the
// shared image.
//
// What it shows: exporting is not what costs. An exported image created the
// documented way, with a DRM format modifier, is copied into at the same speed
// as an ordinary texture and is WRITTEN faster still. score's export, LINEAR
// and in host-visible memory, is slower by orders of magnitude on both, and it
// is the only row that is.
//
// The write column is why score only renders straight into the shared image
// when the layout is tiled. On a LINEAR host-visible export, writing costs MORE
// than copying; on a tiled one it costs a fraction. Same code, opposite
// conclusion, decided by the layout.
//
// The fix it points at: create the exported image with
// VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT from the driver's own list, read back
// the modifier it chose with vkGetImageDrmFormatModifierPropertiesEXT, and
// advertise THAT to pipewire instead of asserting DRM_FORMAT_MOD_LINEAR. It is
// also the honest thing to advertise: today score claims a layout it did not
// ask for.
//
// One caveat this probe cannot settle. score's own 8K frame costs far more
// than even the worst row here, so most of it is in score's path rather than
// in the driver's export, and finding it wants a GPU timestamp around the
// render and the copy separately -- wall-clock around the offscreen frame
// lumps them together.
//
// Build:
//   g++ -std=c++20 -O2 dmabuf-export-bandwidth.cpp -lvulkan -o dmabuf-bw
// Run:
//   ./dmabuf-bw            # 7680x4320, 30 iterations
//   ./dmabuf-bw 3840 2160 100

#include <vulkan/vulkan.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace
{
#define VKCHECK(expr)                                                    \
  do                                                                     \
  {                                                                      \
    const VkResult vkr_ = (expr);                                        \
    if(vkr_ != VK_SUCCESS)                                               \
    {                                                                    \
      std::fprintf(                                                      \
          stderr, "%s:%d: %s failed with VkResult %d\n", __FILE__,       \
          __LINE__, #expr, int(vkr_));                                   \
      std::exit(1);                                                      \
    }                                                                    \
  } while(0)

constexpr VkFormat kFormat = VK_FORMAT_R8G8B8A8_UNORM;

struct Vk
{
  VkInstance instance{};
  VkPhysicalDevice phys{};
  VkDevice dev{};
  VkQueue queue{};
  uint32_t queueFamily{};
  VkCommandPool pool{};
  VkPhysicalDeviceMemoryProperties memProps{};

  PFN_vkGetPhysicalDeviceFormatProperties2 getFormatProps2{};
  PFN_vkGetImageDrmFormatModifierPropertiesEXT getImageModifier{};
};

//! Memory type index satisfying `bits` and `want`, preferring an exact match.
std::optional<uint32_t>
findMemory(const Vk& vk, uint32_t bits, VkMemoryPropertyFlags want)
{
  for(uint32_t i = 0; i < vk.memProps.memoryTypeCount; i++)
    if((bits & (1u << i))
       && (vk.memProps.memoryTypes[i].propertyFlags & want) == want)
      return i;
  return std::nullopt;
}

struct Image
{
  VkImage image{};
  VkDeviceMemory memory{};
  uint64_t modifier{UINT64_MAX};
  bool deviceLocal{};
  bool ok{};
};

//! Every DRM format modifier this driver can render kFormat with.
std::vector<uint64_t> renderableModifiers(const Vk& vk)
{
  VkDrmFormatModifierPropertiesListEXT list{};
  list.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;

  VkFormatProperties2 props{};
  props.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
  props.pNext = &list;
  vk.getFormatProps2(vk.phys, kFormat, &props);

  std::vector<VkDrmFormatModifierPropertiesEXT> mods(
      list.drmFormatModifierCount);
  list.pDrmFormatModifierProperties = mods.data();
  vk.getFormatProps2(vk.phys, kFormat, &props);

  std::vector<uint64_t> out;
  for(const auto& m : mods)
  {
    // Must support being a colour attachment and a transfer destination, or it
    // is no use as a frame we render into and hand over.
    const auto need = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
                      | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if((m.drmFormatModifierTilingFeatures & need) == need)
      out.push_back(m.drmFormatModifier);
  }
  return out;
}

enum class Kind
{
  PlainOptimal,      //!< the control: an ordinary texture, no export
  ExportLinearHost,  //!< what score does, in host-visible memory
  ExportLinearLocal, //!< what score does, forced device-local
  ExportOptimal,     //!< optimal tiling AND exported (often invalid, tried anyway)
  ExportModifier,    //!< the documented path: let the driver pick a modifier
};

const char* name(Kind k)
{
  switch(k)
  {
    case Kind::PlainOptimal:
      return "plain OPTIMAL (no export)";
    case Kind::ExportLinearHost:
      return "export LINEAR, host-visible";
    case Kind::ExportLinearLocal:
      return "export LINEAR, device-local";
    case Kind::ExportOptimal:
      return "export OPTIMAL";
    case Kind::ExportModifier:
      return "export DRM_FORMAT_MODIFIER";
  }
  return "?";
}

Image makeImage(
    const Vk& vk, Kind kind, uint32_t w, uint32_t h,
    const std::vector<uint64_t>& modifiers)
{
  Image out;

  VkExternalMemoryImageCreateInfo ext{};
  ext.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
  ext.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

  VkImageDrmFormatModifierListCreateInfoEXT modList{};
  modList.sType
      = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT;
  modList.drmFormatModifierCount = uint32_t(modifiers.size());
  modList.pDrmFormatModifiers = modifiers.data();

  VkImageCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  ci.imageType = VK_IMAGE_TYPE_2D;
  ci.format = kFormat;
  ci.extent = {w, h, 1};
  ci.mipLevels = 1;
  ci.arrayLayers = 1;
  ci.samples = VK_SAMPLE_COUNT_1_BIT;
  ci.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
             | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
             | VK_IMAGE_USAGE_SAMPLED_BIT;
  ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  switch(kind)
  {
    case Kind::PlainOptimal:
      ci.tiling = VK_IMAGE_TILING_OPTIMAL;
      break;
    case Kind::ExportLinearHost:
    case Kind::ExportLinearLocal:
      ci.tiling = VK_IMAGE_TILING_LINEAR;
      ci.pNext = &ext;
      break;
    case Kind::ExportOptimal:
      ci.tiling = VK_IMAGE_TILING_OPTIMAL;
      ci.pNext = &ext;
      break;
    case Kind::ExportModifier:
      if(modifiers.empty())
        return out;
      ci.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
      modList.pNext = &ext;
      ci.pNext = &modList;
      break;
  }

  if(vkCreateImage(vk.dev, &ci, nullptr, &out.image) != VK_SUCCESS)
    return out;

  VkMemoryRequirements req{};
  vkGetImageMemoryRequirements(vk.dev, out.image, &req);

  const bool wantHost = (kind == Kind::ExportLinearHost);
  auto type = findMemory(
      vk, req.memoryTypeBits,
      wantHost ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
               : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if(!type)
    type = findMemory(vk, req.memoryTypeBits, 0);
  if(!type)
  {
    vkDestroyImage(vk.dev, out.image, nullptr);
    out.image = VK_NULL_HANDLE;
    return out;
  }
  out.deviceLocal = (vk.memProps.memoryTypes[*type].propertyFlags
                     & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                    != 0;

  VkExportMemoryAllocateInfo exportInfo{};
  exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
  exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

  VkMemoryDedicatedAllocateInfo dedicated{};
  dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
  dedicated.image = out.image;
  exportInfo.pNext = &dedicated;

  VkMemoryAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc.allocationSize = req.size;
  alloc.memoryTypeIndex = *type;
  if(kind != Kind::PlainOptimal)
    alloc.pNext = &exportInfo;
  else
    alloc.pNext = &dedicated;

  if(vkAllocateMemory(vk.dev, &alloc, nullptr, &out.memory) != VK_SUCCESS)
  {
    vkDestroyImage(vk.dev, out.image, nullptr);
    out.image = VK_NULL_HANDLE;
    return out;
  }
  VKCHECK(vkBindImageMemory(vk.dev, out.image, out.memory, 0));

  if(kind == Kind::ExportModifier && vk.getImageModifier)
  {
    VkImageDrmFormatModifierPropertiesEXT p{};
    p.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT;
    if(vk.getImageModifier(vk.dev, out.image, &p) == VK_SUCCESS)
      out.modifier = p.drmFormatModifier;
  }

  out.ok = true;
  return out;
}

void destroy(const Vk& vk, Image& img)
{
  if(img.image)
    vkDestroyImage(vk.dev, img.image, nullptr);
  if(img.memory)
    vkFreeMemory(vk.dev, img.memory, nullptr);
  img = {};
}

void barrier(
    VkCommandBuffer cb, VkImage img, VkImageLayout from, VkImageLayout to,
    VkAccessFlags srcAccess, VkAccessFlags dstAccess)
{
  VkImageMemoryBarrier b{};
  b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  b.oldLayout = from;
  b.newLayout = to;
  b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.image = img;
  b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  b.srcAccessMask = srcAccess;
  b.dstAccessMask = dstAccess;
  vkCmdPipelineBarrier(
      cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      0, 0, nullptr, 0, nullptr, 1, &b);
}

//! Milliseconds per full-image CLEAR of dst, averaged over `iters`.
//!
//! The copy below is what score's render-then-copy path does. This is the other
//! shape -- the graphics engine writing every pixel of the destination, which
//! is what rendering INTO the shared image does. A transfer copes with linear
//! memory far better than scattered writes do, so the two rows can disagree by
//! a lot, and which one matters depends on which path the layout allows.
double timeClears(const Vk& vk, VkImage dst, int iters);

//! Milliseconds per copy of src -> dst, averaged over `iters`, GPU waited on.
double timeCopies(
    const Vk& vk, VkImage src, VkImage dst, uint32_t w, uint32_t h, int iters)
{
  VkCommandBufferAllocateInfo cbai{};
  cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cbai.commandPool = vk.pool;
  cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbai.commandBufferCount = 1;
  VkCommandBuffer cb{};
  VKCHECK(vkAllocateCommandBuffers(vk.dev, &cbai, &cb));

  const auto record = [&] {
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VKCHECK(vkBeginCommandBuffer(cb, &bi));

    barrier(
        cb, src, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        0, VK_ACCESS_TRANSFER_READ_BIT);
    barrier(
        cb, dst, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0, VK_ACCESS_TRANSFER_WRITE_BIT);

    VkImageCopy region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.extent = {w, h, 1};
    vkCmdCopyImage(
        cb, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VKCHECK(vkEndCommandBuffer(cb));
  };

  const auto submit = [&] {
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    VKCHECK(vkQueueSubmit(vk.queue, 1, &si, VK_NULL_HANDLE));
    VKCHECK(vkQueueWaitIdle(vk.queue));
  };

  record();
  for(int i = 0; i < 3; i++) // warm
    submit();

  const auto t0 = std::chrono::steady_clock::now();
  for(int i = 0; i < iters; i++)
    submit();
  const auto t1 = std::chrono::steady_clock::now();

  vkFreeCommandBuffers(vk.dev, vk.pool, 1, &cb);
  return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
}

double timeClears(const Vk& vk, VkImage dst, int iters)
{
  VkCommandBufferAllocateInfo cbai{};
  cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cbai.commandPool = vk.pool;
  cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbai.commandBufferCount = 1;
  VkCommandBuffer cb{};
  VKCHECK(vkAllocateCommandBuffers(vk.dev, &cbai, &cb));

  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  VKCHECK(vkBeginCommandBuffer(cb, &bi));
  barrier(
      cb, dst, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
      VK_ACCESS_TRANSFER_WRITE_BIT);
  const VkClearColorValue colour{{0.1f, 0.2f, 0.3f, 1.f}};
  const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCmdClearColorImage(
      cb, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &colour, 1, &range);
  VKCHECK(vkEndCommandBuffer(cb));

  const auto submit = [&] {
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    VKCHECK(vkQueueSubmit(vk.queue, 1, &si, VK_NULL_HANDLE));
    VKCHECK(vkQueueWaitIdle(vk.queue));
  };

  for(int i = 0; i < 3; i++)
    submit();
  const auto t0 = std::chrono::steady_clock::now();
  for(int i = 0; i < iters; i++)
    submit();
  const auto t1 = std::chrono::steady_clock::now();

  vkFreeCommandBuffers(vk.dev, vk.pool, 1, &cb);
  return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
}
}

int main(int argc, char** argv)
{
  // With no arguments: 4K and 8K, the two sizes the question is about.
  std::vector<std::pair<uint32_t, uint32_t>> sizes;
  if(argc > 2)
    sizes.push_back({uint32_t(std::atoi(argv[1])), uint32_t(std::atoi(argv[2]))});
  else
    sizes = {{3840u, 2160u}, {7680u, 4320u}};
  const int iters = argc > 3 ? std::atoi(argv[3]) : 30;

  Vk vk;

  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = "dmabuf-export-bandwidth";
  app.apiVersion = VK_API_VERSION_1_1;

  const char* instExts[] = {
      VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};
  VkInstanceCreateInfo ici{};
  ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ici.pApplicationInfo = &app;
  ici.enabledExtensionCount = 2;
  ici.ppEnabledExtensionNames = instExts;
  VKCHECK(vkCreateInstance(&ici, nullptr, &vk.instance));

  uint32_t nphys = 0;
  VKCHECK(vkEnumeratePhysicalDevices(vk.instance, &nphys, nullptr));
  std::vector<VkPhysicalDevice> physes(nphys);
  VKCHECK(vkEnumeratePhysicalDevices(vk.instance, &nphys, physes.data()));
  if(physes.empty())
  {
    std::fprintf(stderr, "no Vulkan device\n");
    return 1;
  }
  vk.phys = physes.front();
  for(auto p : physes)
  {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(p, &props);
    if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
      vk.phys = p;
      break;
    }
  }

  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(vk.phys, &props);
  vkGetPhysicalDeviceMemoryProperties(vk.phys, &vk.memProps);

  uint32_t nqf = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(vk.phys, &nqf, nullptr);
  std::vector<VkQueueFamilyProperties> qfs(nqf);
  vkGetPhysicalDeviceQueueFamilyProperties(vk.phys, &nqf, qfs.data());
  vk.queueFamily = 0;
  for(uint32_t i = 0; i < nqf; i++)
    if(qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
    {
      vk.queueFamily = i;
      break;
    }

  // Which of the extensions this probe wants are actually present.
  uint32_t ndev = 0;
  VKCHECK(vkEnumerateDeviceExtensionProperties(vk.phys, nullptr, &ndev, nullptr));
  std::vector<VkExtensionProperties> devExts(ndev);
  VKCHECK(
      vkEnumerateDeviceExtensionProperties(vk.phys, nullptr, &ndev, devExts.data()));
  const auto has = [&](const char* n) {
    return std::any_of(devExts.begin(), devExts.end(), [&](const auto& e) {
      return std::strcmp(e.extensionName, n) == 0;
    });
  };

  std::vector<const char*> wanted{
      VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
      VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
      VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME};
  const bool hasModifiers = has(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
  if(hasModifiers)
  {
    wanted.push_back(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
    // Its own dependency, which some drivers do not imply.
    if(has(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME))
      wanted.push_back(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME);
  }
  std::vector<const char*> enabled;
  for(auto* e : wanted)
    if(has(e))
      enabled.push_back(e);

  const float prio = 1.f;
  VkDeviceQueueCreateInfo qci{};
  qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  qci.queueFamilyIndex = vk.queueFamily;
  qci.queueCount = 1;
  qci.pQueuePriorities = &prio;

  VkDeviceCreateInfo dci{};
  dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;
  dci.enabledExtensionCount = uint32_t(enabled.size());
  dci.ppEnabledExtensionNames = enabled.data();
  VKCHECK(vkCreateDevice(vk.phys, &dci, nullptr, &vk.dev));
  vkGetDeviceQueue(vk.dev, vk.queueFamily, 0, &vk.queue);

  VkCommandPoolCreateInfo pci{};
  pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pci.queueFamilyIndex = vk.queueFamily;
  pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  VKCHECK(vkCreateCommandPool(vk.dev, &pci, nullptr, &vk.pool));

  vk.getFormatProps2 = (PFN_vkGetPhysicalDeviceFormatProperties2)
      vkGetInstanceProcAddr(vk.instance, "vkGetPhysicalDeviceFormatProperties2");
  vk.getImageModifier = (PFN_vkGetImageDrmFormatModifierPropertiesEXT)
      vkGetDeviceProcAddr(vk.dev, "vkGetImageDrmFormatModifierPropertiesEXT");

  std::printf(
      "device: %s\n%d iterations per row\n"
      "VK_EXT_image_drm_format_modifier: %s\n\n",
      props.deviceName, iters, hasModifiers ? "yes" : "NO");

  std::vector<uint64_t> modifiers;
  if(hasModifiers && vk.getFormatProps2)
  {
    modifiers = renderableModifiers(vk);
    std::printf("renderable modifiers for RGBA8: %zu\n", modifiers.size());
    for(auto m : modifiers)
      std::printf("  0x%016llx\n", (unsigned long long)m);
    std::printf("\n");
  }

  std::printf(
      "%-30s %8s %10s %10s %10s %10s  %s\n", "destination", "size", "copy ms",
      "copy GB/s", "clear ms", "clear GB/s", "modifier");
  std::printf("%s\n", std::string(96, '-').c_str());

  for(const auto& [W, H] : sizes)
  {
    // The source every copy reads: an ordinary optimal image, as score's own
    // render target is.
    Image src = makeImage(vk, Kind::PlainOptimal, W, H, modifiers);
    if(!src.ok)
    {
      std::fprintf(stderr, "could not create a %ux%u source image\n", W, H);
      continue;
    }

    char sizetxt[16];
    std::snprintf(sizetxt, sizeof sizetxt, "%ux%u", W, H);
    const double bytes = double(W) * H * 4;

    for(const Kind kind :
        {Kind::PlainOptimal, Kind::ExportLinearHost, Kind::ExportLinearLocal,
         Kind::ExportOptimal, Kind::ExportModifier})
    {
      Image dst = makeImage(vk, kind, W, H, modifiers);
      if(!dst.ok)
      {
        std::printf("%-30s %8s %10s\n", name(kind), sizetxt, "unsupported");
        continue;
      }

      const double copyMs = timeCopies(vk, src.image, dst.image, W, H, iters);
      const double clearMs = timeClears(vk, dst.image, iters);

      char modtxt[32] = "-";
      if(dst.modifier != UINT64_MAX)
        std::snprintf(
            modtxt, sizeof modtxt, "0x%llx", (unsigned long long)dst.modifier);

      std::printf(
          "%-30s %8s %10.3f %10.1f %10.3f %10.1f  %s\n", name(kind), sizetxt,
          copyMs, bytes / (copyMs / 1000.) / 1e9, clearMs,
          bytes / (clearMs / 1000.) / 1e9, modtxt);

      destroy(vk, dst);
    }
    destroy(vk, src);
    std::printf("\n");
  }

  vkDestroyCommandPool(vk.dev, vk.pool, nullptr);
  vkDestroyDevice(vk.dev, nullptr);
  vkDestroyInstance(vk.instance, nullptr);
  return 0;
}

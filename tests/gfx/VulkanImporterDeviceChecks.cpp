// L3: each Linux Vulkan importer decides it is usable from what the VkDevice
// enabled, not from what the instance can resolve.
//
// A Qt Quick window's QRhi creates its VkDevice with Qt's own short extension
// list; a TextureSource renders the graph on it. vkGetInstanceProcAddr returns
// a loader trampoline for any device command, so an importer that asks the
// instance reports itself available there and then calls a command of an
// extension the device never enabled. vkGetDeviceProcAddr returns null for
// those. Three devices:
//   - Qt's default extensions (what a Qt Quick window gets): every importer
//     must report unavailable, and an import attempted the way the callers do
//     must not reach the driver (no validation message);
//   - sharedVulkanDeviceExtensions() on a QRhi-created device;
//   - score's own render state (the shared device ScreenNode imports);
//   - score's fallback render state: the QRhi-owned device ScreenNode makes
//     with SCORE_GFX_NO_SHARED_VKDEVICE, or when the shared one cannot be made;
// the last three report available wherever the physical device supports it.
// On the two score devices a DMA-BUF import -- udmabuf with the LINEAR
// modifier, and a GBM buffer with a vendor tiling modifier -- must succeed
// without a validation message.

#include <score/gfx/Vulkan.hpp>

#include <Gfx/Graph/RenderState.hpp>

#include <score_test/App.hpp>
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QtGlobal>

#if defined(__linux__) && QT_HAS_VULKAN && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#define SCORE_TEST_VK_IMPORTERS 1
#include <Gfx/Graph/VulkanVideoDevice.hpp>
#include <Gfx/Graph/decoders/HWCUDA.hpp>
#include <Gfx/Graph/interop/CudaInterop.h>
#include <Gfx/Graph/interop/DMABufImport.hpp>
#include <Gfx/Graph/interop/VkCudaSemaphore.hpp>
#include <Gfx/Graph/interop/VkHostImportUpload.hpp>

#include <QtGui/private/qrhivulkan_p.h>

#include <drm/drm_fourcc.h>
#include <linux/udmabuf.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

extern "C" {
#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
}
#endif

#include <atomic>
#include <cstdio>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#if defined(SCORE_TEST_VK_IMPORTERS)
namespace
{
enum class Device
{
  QtDefault,
  SharedExtensions,
  ScoreRenderState,
  ScoreFallback
};

const char* deviceName(Device d)
{
  switch(d)
  {
    case Device::QtDefault:
      return "Qt default extensions";
    case Device::SharedExtensions:
      return "sharedVulkanDeviceExtensions()";
    case Device::ScoreRenderState:
      return "score render state";
    case Device::ScoreFallback:
      return "score fallback render state (SCORE_GFX_NO_SHARED_VKDEVICE)";
  }
  return "?";
}

struct Verdicts
{
  bool skipped{true};
  std::string skipReason;

  bool dmabuf{};
  bool hostImport{};
  std::optional<bool> cudaDecoder;
  std::optional<bool> cudaSemaphore;

  // Physical-device support: what "available" can at most be on a device that asked for everything.
  bool supportsDmabuf{};
  bool supportsHostImport{};
  bool supportsCudaFd{};
};

std::set<std::string> supportedExtensions(QVulkanInstance& inst, VkPhysicalDevice pd)
{
  auto* f = inst.functions();
  uint32_t n = 0;
  f->vkEnumerateDeviceExtensionProperties(pd, nullptr, &n, nullptr);
  std::vector<VkExtensionProperties> v(n);
  f->vkEnumerateDeviceExtensionProperties(pd, nullptr, &n, v.data());
  std::set<std::string> out;
  for(auto& e : v)
    out.insert(e.extensionName);
  return out;
}

QRhi* createQRhi(QVulkanInstance* inst, bool sharedExtensions)
{
  QRhiVulkanInitParams p;
  p.inst = inst;
  if(sharedExtensions)
    for(const char* e : score::gfx::sharedVulkanDeviceExtensions())
      p.deviceExtensions << e;
  return QRhi::create(QRhi::Vulkan, &p);
}

// Runs @p fn against a QRhi on the requested device; false when it cannot be brought up.
template <typename F>
bool withDevice(Device d, std::string& why, F&& fn)
{
  if(d == Device::ScoreRenderState || d == Device::ScoreFallback)
  {
    // ScreenNode reads the variable each time it makes a render state.
    const bool wasSet = qEnvironmentVariableIsSet("SCORE_GFX_NO_SHARED_VKDEVICE");
    const QByteArray previous = qgetenv("SCORE_GFX_NO_SHARED_VKDEVICE");
    if(d == Device::ScoreFallback)
      qputenv("SCORE_GFX_NO_SHARED_VKDEVICE", "1");
    else
      qunsetenv("SCORE_GFX_NO_SHARED_VKDEVICE");
    auto st = score::gfx::createRenderState(score::gfx::Vulkan, QSize{16, 16}, nullptr);
    if(wasSet)
      qputenv("SCORE_GFX_NO_SHARED_VKDEVICE", previous);
    else
      qunsetenv("SCORE_GFX_NO_SHARED_VKDEVICE");
    if(!st || !st->rhi || st->rhi->backend() != QRhi::Vulkan)
    {
      why = "createRenderState(Vulkan) produced no Vulkan QRhi";
      return false;
    }
    fn(*st->rhi);
    st->destroy();
    return true;
  }
  auto* inst = score::gfx::staticVulkanInstance();
  if(!inst)
  {
    why = "no Vulkan instance";
    return false;
  }
  std::unique_ptr<QRhi> rhi{createQRhi(inst, d == Device::SharedExtensions)};
  if(!rhi)
  {
    why = "QRhi::create(Vulkan) failed";
    return false;
  }
  fn(*rhi);
  return true;
}

AVBufferRef* cudaHwDevice()
{
  AVBufferRef* ref{};
  if(av_hwdevice_ctx_create(&ref, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0) < 0)
    return nullptr;
  return ref;
}

Verdicts probe(Device d)
{
  Verdicts out;
  std::string name;
  if(!score::test::gfx::probe_api(score::gfx::Vulkan, name))
  {
    out.skipReason = "Vulkan unavailable on this host";
    return out;
  }

  const bool ok = withDevice(d, out.skipReason, [&](QRhi& rhi) {
    auto* nh = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
    const auto sup = supportedExtensions(*nh->inst, nh->physDev);
    const auto has = [&](const char* e) { return sup.count(e) > 0; };
    out.supportsDmabuf = has(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME)
                         && has(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME)
                         && has(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
    out.supportsHostImport = has(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
    out.supportsCudaFd = has(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME)
                         && has(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);

    out.dmabuf = score::gfx::DMABufPlaneImporter::isAvailable(rhi);
    out.hostImport = score::gfx::interop::VkHostImportUpload::requiredAlignment(rhi) != 0;
    // Printed as they come: a broken importer may crash the probes after it.
    std::fprintf(stderr, "[importers] %s: dmabuf=%d hostImport=%d\n", deviceName(d), out.dmabuf, out.hostImport);

#if defined(SCORE_HAS_CUDA_HWCONTEXT)
    if(auto* hw = cudaHwDevice())
    {
      out.cudaDecoder = score::gfx::HWCudaVulkanDecoder::isAvailable(rhi, hw);
      std::fprintf(stderr, "[importers] %s: cudaDecoder=%d\n", deviceName(d), *out.cudaDecoder);
      av_buffer_unref(&hw);
    }
#endif
    CudaInteropContextHandle cuda{};
    if(cuda_interop_available() && cuda_interop_init(&cuda) == CUDA_INTEROP_SUCCESS)
    {
      score::gfx::vkinterop::VulkanCtx vk{
          nh->inst->vkInstance(), nh->physDev, nh->dev, nh->inst};
      score::gfx::vkinterop::VkCudaTimelineSemaphore sem;
      out.cudaSemaphore = sem.create(vk, cuda, 0, /*binary=*/true);
      std::fprintf(stderr, "[importers] %s: cudaSemaphore=%d\n", deviceName(d), *out.cudaSemaphore);
      sem.destroy();
      cuda_interop_shutdown(cuda);
    }
  });
  out.skipped = !ok;
  return out;
}

// Validation-layer messages seen while the handler is installed.
std::atomic<int> g_validationMessages{0};
QtMessageHandler g_previousHandler{};
void countValidation(QtMsgType t, const QMessageLogContext& c, const QString& m)
{
  if(m.contains("Validation Error"))
    ++g_validationMessages;
  if(g_previousHandler)
    g_previousHandler(t, c, m);
}

// A dma-buf of @p bytes from udmabuf; -1 when the host has none.
int makeDmaBuf(std::size_t bytes)
{
  const int dev = ::open("/dev/udmabuf", O_RDWR | O_CLOEXEC);
  if(dev < 0)
    return -1;
  const int mfd = ::memfd_create("score-test-dmabuf", MFD_ALLOW_SEALING);
  int buf = -1;
  if(mfd >= 0 && ::ftruncate(mfd, bytes) == 0
     && ::fcntl(mfd, F_ADD_SEALS, F_SEAL_SHRINK) == 0)
  {
    udmabuf_create c{};
    c.memfd = mfd;
    c.size = bytes;
    buf = ::ioctl(dev, UDMABUF_CREATE, &c);
  }
  if(mfd >= 0)
    ::close(mfd);
  ::close(dev);
  return buf;
}

// Every import the callers would attempt on @p rhi, gated the way they gate it.
void attemptImports(QRhi& rhi, bool& dmabufAttempted, bool& hostAttempted)
{
  namespace interop = score::gfx::interop;
  if(const auto align = interop::VkHostImportUpload::requiredAlignment(rhi))
  {
    hostAttempted = true;
    const std::size_t bytes = 64 * 1024;
    void* host = interop::alignedSlotAlloc(bytes, align);
    interop::VkHostImportedBuffer buf;
    if(interop::importHostPointerBuffer(
           rhi, host, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, false, buf))
      interop::releaseHostImportedBuffer(rhi, buf);
    interop::alignedSlotFree(host);
  }

  if(score::gfx::DMABufPlaneImporter::isAvailable(rhi))
  {
    const int fd = makeDmaBuf(64 * 64 * 4);
    if(fd >= 0)
    {
      dmabufAttempted = true;
      score::gfx::DMABufPlaneImporter imp;
      imp.init(rhi);
      score::gfx::DMABufPlaneImporter::PlaneImport plane;
      if(imp.importPlane(plane, fd, 0 /*DRM_FORMAT_MOD_LINEAR*/, 0, 64 * 4,
                         VK_FORMAT_R8G8B8A8_UNORM, 64, 64))
        imp.cleanupPlane(plane);
      ::close(fd);
    }
  }
}

// libgbm, dlopen'd so the test needs neither its headers nor a link to it.
struct Gbm
{
  struct device;
  struct bo;
  void* lib{};
  device* (*createDevice)(int){};
  void (*destroyDevice)(device*){};
  bo* (*createWithModifiers)(
      device*, uint32_t, uint32_t, uint32_t, const uint64_t*, unsigned){};
  void (*destroyBo)(bo*){};
  uint64_t (*modifier)(bo*){};
  int (*planeCount)(bo*){};
  uint32_t (*stride)(bo*, int){};
  uint32_t (*offset)(bo*, int){};
  int (*fd)(bo*){};

  bool load()
  {
    if(!(lib = ::dlopen("libgbm.so.1", RTLD_NOW | RTLD_LOCAL)))
      return false;
    const auto sym = [&](auto& f, const char* name) {
      f = reinterpret_cast<std::remove_reference_t<decltype(f)>>(::dlsym(lib, name));
      return f != nullptr;
    };
    return sym(createDevice, "gbm_create_device")
           && sym(destroyDevice, "gbm_device_destroy")
           && sym(createWithModifiers, "gbm_bo_create_with_modifiers")
           && sym(destroyBo, "gbm_bo_destroy") && sym(modifier, "gbm_bo_get_modifier")
           && sym(planeCount, "gbm_bo_get_plane_count")
           && sym(stride, "gbm_bo_get_stride_for_plane")
           && sym(offset, "gbm_bo_get_offset") && sym(fd, "gbm_bo_get_fd");
  }
  ~Gbm()
  {
    if(lib)
      ::dlclose(lib);
  }
};

// Single-plane, non-LINEAR modifiers with which the device can import a
// sampled R8G8B8A8 DMA-BUF image.
std::vector<uint64_t> vendorModifiers(QRhi& rhi)
{
  auto* nh = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
  auto* inst = nh->inst;
  auto fmtProps2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties2>(
      inst->getInstanceProcAddr("vkGetPhysicalDeviceFormatProperties2"));
  auto imgProps2 = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
      inst->getInstanceProcAddr("vkGetPhysicalDeviceImageFormatProperties2"));
  if(!fmtProps2 || !imgProps2)
    return {};

  VkDrmFormatModifierPropertiesListEXT list{};
  list.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;
  VkFormatProperties2 fp{};
  fp.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
  fp.pNext = &list;
  fmtProps2(nh->physDev, VK_FORMAT_R8G8B8A8_UNORM, &fp);
  std::vector<VkDrmFormatModifierPropertiesEXT> mods(list.drmFormatModifierCount);
  list.pDrmFormatModifierProperties = mods.data();
  fmtProps2(nh->physDev, VK_FORMAT_R8G8B8A8_UNORM, &fp);

  std::vector<uint64_t> out;
  for(const auto& m : mods)
  {
    if(m.drmFormatModifier == DRM_FORMAT_MOD_LINEAR
       || m.drmFormatModifier == DRM_FORMAT_MOD_INVALID
       || m.drmFormatModifierPlaneCount != 1
       || !(m.drmFormatModifierTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
      continue;

    VkPhysicalDeviceImageDrmFormatModifierInfoEXT modInfo{};
    modInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT;
    modInfo.drmFormatModifier = m.drmFormatModifier;
    modInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkPhysicalDeviceExternalImageFormatInfo extInfo{};
    extInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
    extInfo.pNext = &modInfo;
    extInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    VkPhysicalDeviceImageFormatInfo2 info{};
    info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    info.pNext = &extInfo;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.type = VK_IMAGE_TYPE_2D;
    info.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    VkExternalImageFormatProperties extProps{};
    extProps.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
    VkImageFormatProperties2 props{};
    props.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
    props.pNext = &extProps;
    if(imgProps2(nh->physDev, &info, &props) == VK_SUCCESS
       && (extProps.externalMemoryProperties.externalMemoryFeatures
           & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))
      out.push_back(m.drmFormatModifier);
  }
  return out;
}

// The render node of the physical device @p rhi runs on; empty when unknown.
std::string renderNode(QRhi& rhi)
{
#if defined(VK_EXT_physical_device_drm)
  auto* nh = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
  if(!supportedExtensions(*nh->inst, nh->physDev)
          .count(VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME))
    return {};
  auto props2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
      nh->inst->getInstanceProcAddr("vkGetPhysicalDeviceProperties2"));
  if(!props2)
    return {};
  VkPhysicalDeviceDrmPropertiesEXT drm{};
  drm.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT;
  VkPhysicalDeviceProperties2 p{};
  p.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  p.pNext = &drm;
  props2(nh->physDev, &p);
  if(!drm.hasRender)
    return {};
  return "/dev/dri/renderD" + std::to_string(drm.renderMinor);
#else
  (void)rhi;
  return {};
#endif
}

struct DmaBufImportResult
{
  bool available{};
  bool supported{};
  // Unset: not attempted, for the reason in *Skip.
  std::optional<bool> linear;
  std::string linearSkip;
  std::optional<bool> vendor;
  uint64_t vendorModifier{};
  std::string vendorSkip;
};

// A udmabuf (LINEAR) and a GBM buffer (vendor modifier) imported the way
// the DMA-BUF decoders and PipeWire capture import a plane.
void importDmaBufs(QRhi& rhi, DmaBufImportResult& r)
{
  auto* nh = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
  const auto sup = supportedExtensions(*nh->inst, nh->physDev);
  r.supported = sup.count(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME)
                && sup.count(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME)
                && sup.count(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
  r.available = score::gfx::DMABufPlaneImporter::isAvailable(rhi);
  if(!r.available)
  {
    r.linearSkip = r.vendorSkip = "DMA-BUF import unavailable on this device";
    return;
  }

  score::gfx::DMABufPlaneImporter imp;
  imp.init(rhi);
  const auto importOne = [&](int fd, uint64_t mod, uint32_t offset, uint32_t pitch) {
    score::gfx::DMABufPlaneImporter::PlaneImport plane;
    const bool ok = imp.importPlane(
        plane, fd, mod, offset, pitch, VK_FORMAT_R8G8B8A8_UNORM, 64, 64);
    if(ok)
      imp.cleanupPlane(plane);
    return ok;
  };

  if(const int fd = makeDmaBuf(64 * 64 * 4); fd >= 0)
  {
    r.linear = importOne(fd, DRM_FORMAT_MOD_LINEAR, 0, 64 * 4);
    ::close(fd);
  }
  else
    r.linearSkip = "no /dev/udmabuf";

  const auto mods = vendorModifiers(rhi);
  const auto node = renderNode(rhi);
  Gbm gbm;
  if(mods.empty())
  {
    r.vendorSkip = "no importable single-plane non-LINEAR R8G8B8A8 modifier";
    return;
  }
  if(node.empty())
  {
    r.vendorSkip = "render node unknown (VK_EXT_physical_device_drm)";
    return;
  }
  if(!gbm.load())
  {
    r.vendorSkip = "no libgbm.so.1";
    return;
  }
  const int drmFd = ::open(node.c_str(), O_RDWR | O_CLOEXEC);
  if(drmFd < 0)
  {
    r.vendorSkip = "cannot open " + node;
    return;
  }
  if(auto* dev = gbm.createDevice(drmFd))
  {
    if(auto* bo = gbm.createWithModifiers(
           dev, 64, 64, DRM_FORMAT_ABGR8888, mods.data(), unsigned(mods.size())))
    {
      r.vendorModifier = gbm.modifier(bo);
      const int fd = gbm.fd(bo);
      if(r.vendorModifier == DRM_FORMAT_MOD_LINEAR
         || r.vendorModifier == DRM_FORMAT_MOD_INVALID)
        r.vendorSkip = "gbm chose a LINEAR or implicit modifier";
      else if(gbm.planeCount(bo) != 1 || fd < 0)
        r.vendorSkip = "gbm returned a multi-plane buffer or no fd";
      else
        r.vendor = importOne(fd, r.vendorModifier, gbm.offset(bo, 0), gbm.stride(bo, 0));
      if(fd >= 0)
        ::close(fd);
      gbm.destroyBo(bo);
    }
    else
      r.vendorSkip = "gbm_bo_create_with_modifiers failed on " + node;
    gbm.destroyDevice(dev);
  }
  else
    r.vendorSkip = "gbm_create_device failed on " + node;
  ::close(drmFd);
}
}

TEST_CASE("Vulkan importers follow the device's enabled extensions",
          "[gfx][interop][vulkan]")
{
  for(Device d : {Device::QtDefault, Device::SharedExtensions, Device::ScoreRenderState,
                  Device::ScoreFallback})
  {
    Verdicts r;
    score::test::run_in_gui_app([&](const score::GUIApplicationContext&) { r = probe(d); });
    if(r.skipped)
      SKIP(deviceName(d) << ": " << r.skipReason);

    INFO("device: " << deviceName(d));
    INFO("CUDA decoder probed: " << r.cudaDecoder.has_value()
                                 << ", CUDA semaphore probed: " << r.cudaSemaphore.has_value());
    if(d == Device::QtDefault)
    {
      CHECK_FALSE(r.dmabuf);
      CHECK_FALSE(r.hostImport);
      if(r.cudaDecoder)
        CHECK_FALSE(*r.cudaDecoder);
      if(r.cudaSemaphore)
        CHECK_FALSE(*r.cudaSemaphore);
    }
    else
    {
      CHECK(r.dmabuf == r.supportsDmabuf);
      CHECK(r.hostImport == r.supportsHostImport);
      if(r.cudaDecoder)
        CHECK(*r.cudaDecoder == r.supportsCudaFd);
      if(r.cudaSemaphore)
        CHECK(*r.cudaSemaphore == r.supportsCudaFd);
    }
  }
}

TEST_CASE("No import reaches a Qt-default VkDevice",
          "[gfx][interop][vulkan][validation]")
{
  int messages = -1;
  bool dmabufAttempted = false, hostAttempted = false;
  std::string why;
  bool ran = false;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    std::string name;
    if(!score::test::gfx::probe_api(score::gfx::Vulkan, name))
    {
      why = "Vulkan unavailable on this host";
      return;
    }
    g_validationMessages = 0;
    g_previousHandler = qInstallMessageHandler(countValidation);
    ran = withDevice(Device::QtDefault, why, [&](QRhi& rhi) {
      attemptImports(rhi, dmabufAttempted, hostAttempted);
    });
    qInstallMessageHandler(g_previousHandler);
    messages = g_validationMessages;
  });
  if(!ran)
    SKIP(why);

  INFO("SCORE_GPU_VALIDATION=" << qgetenv("SCORE_GPU_VALIDATION").toStdString());
  CHECK_FALSE(hostAttempted);
  CHECK_FALSE(dmabufAttempted);
  CHECK(messages == 0);
}

TEST_CASE("DMA-BUF imports cleanly on score's shared and fallback VkDevices",
          "[gfx][interop][vulkan][validation][dmabuf]")
{
  for(Device d : {Device::ScoreRenderState, Device::ScoreFallback})
  {
    DmaBufImportResult r;
    int messages = -1;
    std::string why;
    bool ran = false;
    score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
      std::string name;
      if(!score::test::gfx::probe_api(score::gfx::Vulkan, name))
      {
        why = "Vulkan unavailable on this host";
        return;
      }
      g_validationMessages = 0;
      g_previousHandler = qInstallMessageHandler(countValidation);
      ran = withDevice(d, why, [&](QRhi& rhi) { importDmaBufs(rhi, r); });
      qInstallMessageHandler(g_previousHandler);
      messages = g_validationMessages;
    });
    if(!ran)
      SKIP(deviceName(d) << ": " << why);

    std::fprintf(
        stderr,
        "[dmabuf] %s: available=%d supported=%d linear=%s%s vendor=%s%s "
        "modifier=%#llx validation=%d\n",
        deviceName(d), r.available, r.supported,
        r.linear ? (*r.linear ? "ok" : "FAILED") : "skipped: ", r.linearSkip.c_str(),
        r.vendor ? (*r.vendor ? "ok" : "FAILED") : "skipped: ", r.vendorSkip.c_str(),
        (unsigned long long)r.vendorModifier, messages);

    INFO("device: " << deviceName(d));
    INFO("SCORE_GPU_VALIDATION=" << qgetenv("SCORE_GPU_VALIDATION").toStdString());
    CHECK(r.available == r.supported);
    if(r.linear)
      CHECK(*r.linear);
    if(r.vendor)
      CHECK(*r.vendor);
    CHECK(messages == 0);
  }
}
#else
TEST_CASE("Vulkan importers follow the device's enabled extensions",
          "[gfx][interop][vulkan]")
{
  SKIP("Linux Vulkan importers only (Qt >= 6.6)");
}
#endif

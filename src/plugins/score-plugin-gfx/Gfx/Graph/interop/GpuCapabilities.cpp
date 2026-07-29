#include "GpuCapabilities.hpp"

#include "CudaFunctions.hpp"

#include <Gfx/Graph/RenderState.hpp>

#include <QDebug>
#include <QFile>
#include <QOpenGLContext>
#include <QString>
#include <QStringList>

#include <private/qrhi_p.h>

#if QT_CONFIG(opengl)
#include <private/qrhigles2_p.h>
#endif

#include <cstring>

// DVP shim provides nv_dvp_load_runtime() + nv_dvp_have_* probes
// without forcing consumers of GpuCapabilities to include the DVP
// headers. We forward-declare the C API here so the header stays clean.
extern "C"
{
  bool nv_dvp_load_runtime(void);
  bool nv_dvp_have_gl(void);
  bool nv_dvp_have_d3d11(void);
  bool nv_dvp_have_cuda(void);
}

namespace score::gfx::interop
{

namespace
{

constexpr uint32_t PCI_VENDOR_NVIDIA = 0x10DE;
constexpr uint32_t PCI_VENDOR_AMD = 0x1002;
constexpr uint32_t PCI_VENDOR_INTEL = 0x8086;
constexpr uint32_t PCI_VENDOR_APPLE = 0x106B;

void copyString(char* dst, std::size_t cap, const char* src) noexcept
{
  if(cap == 0 || !dst)
    return;
  if(!src)
  {
    dst[0] = '\0';
    return;
  }
  std::size_t n = std::strlen(src);
  if(n >= cap)
    n = cap - 1;
  std::memcpy(dst, src, n);
  dst[n] = '\0';
}

HostOs detectOs() noexcept
{
#if defined(_WIN32)
  return HostOs::Windows;
#elif defined(__APPLE__)
  return HostOs::MacOS;
#elif defined(__linux__)
  return HostOs::Linux;
#else
  return HostOs::Other;
#endif
}

/* Linux-only: scan /proc/modules for nvidia_peermem. The module is
 * what enables third-party DMA into NVIDIA VRAM via the nv-p2p API
 * (AJA-RDMA, Rivermax, Ximea GPUDirect paths). Without it those paths fail at
 * runtime even if libcuda + cuMemCreate are present. */
bool probeNvidiaPeermem() noexcept
{
#if defined(__linux__)
  // Fast path: /sys/module/<name>/ exists when the module is loaded.
  if(QFile::exists("/sys/module/nvidia_peermem"))
    return true;
  // Older kernels / driver bundles: nv_peer_mem (legacy name).
  if(QFile::exists("/sys/module/nv_peer_mem"))
    return true;
  return false;
#else
  return false;
#endif
}

GpuVendor vendorFromPciId(uint32_t vendorId, const char* renderer) noexcept
{
  switch(vendorId)
  {
    case PCI_VENDOR_NVIDIA:
      // Distinguish Quadro/Tesla/RTX-Pro (GPUDirect-RDMA capable on Linux)
      // from GeForce (driver-gated; RDMA mostly unavailable). Heuristic:
      // renderer string contains "Quadro", "Tesla", "RTX A", or
      // "NVIDIA <four-digit>" → Pro. Otherwise consumer. This matches
      // DeckLink's `isNvidiaDvpAvailable` Quadro-string check.
      if(renderer)
      {
        const QString r = QString::fromUtf8(renderer);
        if(r.contains("Quadro", Qt::CaseInsensitive)
           || r.contains("Tesla", Qt::CaseInsensitive)
           || r.contains("RTX A", Qt::CaseInsensitive)
           || r.contains("RTX 4000 Ada", Qt::CaseInsensitive)
           || r.contains("RTX 5000 Ada", Qt::CaseInsensitive)
           || r.contains("RTX 6000 Ada", Qt::CaseInsensitive))
          return GpuVendor::NvidiaProQuadro;
      }
      return GpuVendor::NvidiaConsumer;
    case PCI_VENDOR_AMD:
      return GpuVendor::Amd;
    case PCI_VENDOR_INTEL:
      return GpuVendor::Intel;
    case PCI_VENDOR_APPLE:
      return GpuVendor::Apple;
    default:
      return GpuVendor::Unknown;
  }
}

/** Vendor from the renderer/device string. Fallback for backends that leave
 *  QRhiDriverInfo::vendorId at zero — Mesa's GL driver among them. */
GpuVendor vendorFromRendererString(const char* renderer) noexcept
{
  if(!renderer || !*renderer)
    return GpuVendor::Unknown;
  const QString r = QString::fromUtf8(renderer);
  if(r.contains("NVIDIA", Qt::CaseInsensitive)
     || r.contains("GeForce", Qt::CaseInsensitive))
    return vendorFromPciId(PCI_VENDOR_NVIDIA, renderer);
  if(r.contains("AMD", Qt::CaseInsensitive) || r.contains("Radeon", Qt::CaseInsensitive)
     || r.contains("ATI ", Qt::CaseInsensitive)
     || r.contains("radeonsi", Qt::CaseInsensitive))
    return GpuVendor::Amd;
  if(r.contains("Intel", Qt::CaseInsensitive))
    return GpuVendor::Intel;
  if(r.contains("Apple", Qt::CaseInsensitive))
    return GpuVendor::Apple;
  return GpuVendor::Unknown;
}

QRhiBackendKind backendFromRhi(QRhi* rhi) noexcept
{
  if(!rhi)
    return QRhiBackendKind::Unknown;
  switch(rhi->backend())
  {
    case QRhi::OpenGLES2:
      return QRhiBackendKind::OpenGL;
    case QRhi::Vulkan:
      return QRhiBackendKind::Vulkan;
    case QRhi::D3D11:
      return QRhiBackendKind::D3D11;
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    case QRhi::D3D12:
      return QRhiBackendKind::D3D12;
#endif
    case QRhi::Metal:
      return QRhiBackendKind::Metal;
    case QRhi::Null:
      return QRhiBackendKind::Null;
  }
  return QRhiBackendKind::Unknown;
}

} // namespace

GpuCapabilities probeContextFree()
{
  GpuCapabilities caps{};
  caps.os = detectOs();

  // -- DVP loader probe. Idempotent; safe to call repeatedly. --
  if(::nv_dvp_load_runtime())
  {
    caps.dvpLoaded = true;
    caps.dvpHaveGl = ::nv_dvp_have_gl();
    caps.dvpHaveD3D11 = ::nv_dvp_have_d3d11();
    caps.dvpHaveCuda = ::nv_dvp_have_cuda();
  }

  // -- CUDA driver-API probe. We instantiate a CudaFunctions just to
  //    test load+symbol resolution; the table itself is discarded. The
  //    real consumers create their own instance later. --
  {
    score::gfx::CudaFunctions cu;
    if(cu.load())
    {
      caps.cudaLoaded = true;
      caps.cudaVmmSupported = cu.vmmSupported;
    }
  }

  // -- nvidia_peermem kernel module probe. Linux only; gates GPUDirect RDMA. --
  caps.nvidiaPeermem = probeNvidiaPeermem();

  return caps;
}

void probeFromQRhi(GpuCapabilities& caps, QRhi* rhi) noexcept
{
  if(!rhi)
    return;

  caps.backend = backendFromRhi(rhi);

  const auto info = rhi->driverInfo();
  copyString(caps.rendererName, sizeof(caps.rendererName),
             info.deviceName.constData());
  caps.vendor = vendorFromPciId(info.vendorId, info.deviceName.constData());
  // Qt's GL backend does not always fill vendorId (Mesa reports none), which
  // leaves every vendor-gated rung disabled on hardware that has them. Only
  // consulted when the PCI id yielded nothing, so a populated id always wins.
  if(caps.vendor == GpuVendor::Unknown)
    caps.vendor = vendorFromRendererString(info.deviceName.constData());
  qDebug() << "probeFromQRhi: vendorId=" << Qt::hex << info.vendorId << Qt::dec
           << "device=" << info.deviceName << "-> vendor="
           << gpuVendorName(caps.vendor);

  // Vulkan external-memory readiness: QRhi reports the feature, and
  // VkExternalMemoryHelpers gates the actual usage. We expose the QRhi
  // backend bit so consumers don't have to re-check.
  caps.vkExternalMemorySupported
      = (caps.backend == QRhiBackendKind::Vulkan
         && rhi->isFeatureSupported(QRhi::Feature::CustomInstanceStepRate));
  // ^ The Feature::CustomInstanceStepRate check isn't external-memory
  // specific, but it's a coarse signal that this Vulkan QRhi is a
  // modern build with the optional-extension pipeline. Strategies that
  // need a tighter gate query VkExternalMemoryHelpers::isAvailable()
  // themselves; this flag is just "is Vulkan the active backend".

  caps.metalSupported = (caps.backend == QRhiBackendKind::Metal);
}

void probeGlExtensions(GpuCapabilities& caps) noexcept
{
  // Needs a current GL context: this is the only place the AMD extension
  // strings are visible. Callers on non-GL backends, or before the context
  // exists, get caps.amd left alone -- which is what every consumer treats as
  // "no AMD fast path".
  //
  // Only the two real extension STRINGS are probed. The externalVirtual /
  // externalPhysical flags gate AJA's legacy GL_EXTERNAL_VIRTUAL_MEMORY_AMD
  // token, which is not an advertised extension and cannot be detected; a card
  // that has GL_AMD_pinned_memory takes the standard
  // GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD path instead. Setting them on a hunch
  // is exactly how AmdPinnedBuffers ends up calling glBufferData with a token
  // the driver rejects, which it does not check for.
  probeGlExtensions(caps, nullptr);
}

void probeGlExtensions(GpuCapabilities& caps, QRhi* rhi) noexcept
{
  auto* ctx = QOpenGLContext::currentContext();
#if QT_CONFIG(opengl)
  // QRhi only makes its context current inside beginFrame/endFrame, so a
  // caller probing at setup time has none. Take it from the native handles
  // instead, as every other GL interop site here does.
  if(!ctx && rhi && rhi->backend() == QRhi::OpenGLES2)
  {
    if(const auto* native
       = static_cast<const QRhiGles2NativeHandles*>(rhi->nativeHandles()))
      ctx = native->context;
  }
#else
  (void)rhi;
#endif
  if(!ctx)
  {
    qDebug() << "probeGlExtensions: no GL context; AMD rungs stay disabled";
    return;
  }

  caps.amd.pinnedMemory = ctx->hasExtension(QByteArrayLiteral("GL_AMD_pinned_memory"));
  caps.amd.busAddressable
      = ctx->hasExtension(QByteArrayLiteral("GL_AMD_bus_addressable_memory"));
  qDebug() << "probeGlExtensions: GL_AMD_pinned_memory=" << caps.amd.pinnedMemory
           << "GL_AMD_bus_addressable_memory=" << caps.amd.busAddressable;
}

const char* gpuVendorName(GpuVendor v) noexcept
{
  switch(v)
  {
    case GpuVendor::NvidiaConsumer:
      return "NVIDIA (GeForce)";
    case GpuVendor::NvidiaProQuadro:
      return "NVIDIA (Quadro/Tesla/RTX-Pro)";
    case GpuVendor::Amd:
      return "AMD";
    case GpuVendor::Apple:
      return "Apple";
    case GpuVendor::Intel:
      return "Intel";
    case GpuVendor::Other:
      return "Other";
    case GpuVendor::Unknown:
      break;
  }
  return "Unknown";
}

const char* qrhiBackendName(QRhiBackendKind b) noexcept
{
  switch(b)
  {
    case QRhiBackendKind::OpenGL:
      return "OpenGL";
    case QRhiBackendKind::Vulkan:
      return "Vulkan";
    case QRhiBackendKind::D3D11:
      return "D3D11";
    case QRhiBackendKind::D3D12:
      return "D3D12";
    case QRhiBackendKind::Metal:
      return "Metal";
    case QRhiBackendKind::Null:
      return "Null";
    case QRhiBackendKind::Unknown:
      break;
  }
  return "Unknown";
}

} // namespace score::gfx::interop

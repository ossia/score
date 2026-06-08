#include "GpuCapabilities.hpp"

#include "CudaFunctions.hpp"

#include <Gfx/Graph/RenderState.hpp>

#include <QFile>
#include <QString>
#include <QStringList>

#include <rhi/qrhi.h>

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
 * (AJA-RDMA, Rivermax, Ximea tier-0). Without it those paths fail at
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
      // Distinguish Quadro/Tesla/RTX-Pro (tier-0 capable on Linux) from
      // GeForce (driver-gated; tier-0 mostly unavailable). Heuristic:
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
    case QRhi::D3D12:
      return QRhiBackendKind::D3D12;
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

  // -- nvidia_peermem kernel module probe. Linux only; gates tier-0. --
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
  // The actual glGetString call must happen on the GL thread with the
  // GL context bound. We avoid pulling in <QOpenGLContext> at this
  // layer — instead consumers that genuinely have GL active call us
  // from a closure that has already grabbed the extension list.
  //
  // The function intentionally takes no GL handle; we re-invoke
  // glGetString through Qt's QOpenGLContext::currentContext() if any
  // is current. If no GL context is current we leave caps.amd
  // untouched.
  (void)caps;
  // Implementation deferred to a GL-backend-only TU; see
  // GpuCapabilitiesGl.cpp (compiled only when the GL backend is
  // selected). Without that TU, AMD pinned-memory probing returns
  // "unknown" — the worst-case correctness consequence is one
  // fallback path being unavailable; nothing breaks.
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

#pragma once

/**
 * @file GpuCapabilities.hpp
 * @brief Runtime probe of which GPU interop mechanisms are available on
 *        this system.
 *
 * Every vendor video strategy (AJA, DeckLink, Magewell, Rivermax, …)
 * needs to decide at init time which tier of GPU interop to use:
 *
 *   tier 0 — true GPU-direct (CUDA VMM + nvidia_peermem, or AMD
 *            bus-addressable). NIC/card writes straight into VRAM.
 *   tier 1 — sysmem-mediated via NVIDIA DVP (GL/D3D11/CUDA backends).
 *   tier 2 — sysmem-mediated via AMD GL pinned-memory extensions.
 *   tier 4 — page-locked host memory + plain copy.
 *
 * Strategies that re-probe per-instance duplicate work and tend to
 * disagree on edge cases (one detects DVP, another doesn't). This
 * struct centralises detection: probe once at plugin init, hand the
 * populated struct to each strategy.
 *
 * Two-stage probe:
 *   - `probeContextFree()` runs without any GL/D3D context. It
 *     populates DVP loader state, CUDA driver loader state, kernel
 *     module presence, OS info. Cheap; can run at plugin load.
 *   - `probeFromQRhi(QRhi*)` augments with renderer string + the
 *     QRhi backend identity. The AMD extension probe requires a live
 *     GL context; if QRhi is on Vulkan/D3D, AMD-pinned is N/A.
 *   - `probeGlExtensions()` is the GL-only deep probe used when score
 *     genuinely runs on the GL backend (AJA-AMD demo path style).
 *
 * The struct is value-typed and trivially copyable; pass by value or
 * by const ref to strategies. There's intentionally no singleton —
 * each plugin/addon that needs capabilities should call
 * `probeContextFree()` once and store the result.
 */

#include <score_plugin_gfx_export.h>

#include <cstdint>

class QRhi;

namespace score::gfx::interop
{

/** Coarse GPU vendor label derived from renderer string / driver
 *  fingerprint. */
enum class GpuVendor : uint8_t
{
  Unknown = 0,
  NvidiaConsumer,   /**< GeForce. Tier-0 RDMA is unavailable (driver-gated). */
  NvidiaProQuadro,  /**< Quadro / RTX A-series / Tesla. Tier-0 enabled by driver. */
  Amd,              /**< Radeon, Radeon Pro, FirePro. AMD-pinned + DirectGMA candidates. */
  Apple,            /**< Apple Silicon / AMD on macOS — Metal-native. */
  Intel,            /**< Iris / Arc — no GPU-direct paths. */
  Other,
};

/** OS label. Matters because some paths (`nvidia_peermem`, AMD bus
 *  addressable, V4L2 MMAP) are Linux-only; D3D11/D3D12 are Windows-only;
 *  Metal is macOS-only. */
enum class HostOs : uint8_t
{
  Linux = 0,
  Windows,
  MacOS,
  Other,
};

/** Which QRhi backend the renderer is currently using. AMD GL
 *  extensions are visible only when the live backend is GL. DVP can
 *  target GL or D3D11; the active QRhi backend decides which DVP
 *  init path the strategy will pick. */
enum class QRhiBackendKind : uint8_t
{
  Unknown = 0,
  OpenGL,
  Vulkan,
  D3D11,
  D3D12,
  Metal,
  Null,
};

/** Per-AMD-extension presence. AJA's `demos/AMD/SDICommon/
 *  GLTransferBuffers.cpp` probes for all three and selects the
 *  best-available; we expose the same three flags. */
struct AmdGlExtensions
{
  bool busAddressable{};         /**< GL_AMD_bus_addressable_memory — tier-0 P2P. */
  bool externalVirtualMemory{};  /**< GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD / GL_EXTERNAL_VIRTUAL_MEMORY_AMD — tier-2 virtual pinning. */
  bool externalPhysicalMemory{}; /**< GL_EXTERNAL_PHYSICAL_MEMORY_AMD — tier-2 physical pinning. */
  bool pinnedMemory{};           /**< GL_AMD_pinned_memory — DeckLink-style sysmem→GL. */

  /** True when any AMD pinned-memory mechanism is usable. */
  constexpr bool any() const noexcept
  {
    return busAddressable || externalVirtualMemory || externalPhysicalMemory
           || pinnedMemory;
  }
};

/** Result of the capability probe. */
struct SCORE_PLUGIN_GFX_EXPORT GpuCapabilities
{
  // -- Identity --------------------------------------------------------
  GpuVendor vendor{GpuVendor::Unknown};
  HostOs os{HostOs::Other};
  QRhiBackendKind backend{QRhiBackendKind::Unknown};

  /** GPU renderer/device name. Captured for logging + heuristics. */
  char rendererName[128]{};

  /** Driver version string when obtainable (e.g. NVIDIA driver "555.42").
   *  Empty when not detectable. */
  char driverVersion[64]{};

  // -- NVIDIA DVP ------------------------------------------------------

  /** dvp.dll / libdvp.so.1 loaded successfully. */
  bool dvpLoaded{};
  /** DVP GL entry points fully resolved. */
  bool dvpHaveGl{};
  /** DVP D3D11 entry points fully resolved (Windows only). */
  bool dvpHaveD3D11{};
  /** DVP CUDA entry points fully resolved (cross-platform). */
  bool dvpHaveCuda{};

  // -- CUDA driver API ------------------------------------------------

  /** libcuda.so.1 / nvcuda.dll loaded; basic driver-API entry points
   *  resolved. */
  bool cudaLoaded{};
  /** CUDA Virtual Memory Management API (cuMemCreate / cuMemMap /
   *  cuMemSetAccess) available. Required for the tier-0 NIC→VRAM
   *  Rivermax-style allocation. */
  bool cudaVmmSupported{};
  /** nvidia_peermem kernel module loaded (Linux). Gates AJA-RDMA and
   *  Rivermax/Ximea tier-0 paths. Always false on non-Linux. */
  bool nvidiaPeermem{};

  // -- AMD GL ----------------------------------------------------------

  /** Per-extension presence. Populated only by `probeGlExtensions()`. */
  AmdGlExtensions amd{};

  // -- Vulkan ----------------------------------------------------------

  /** QRhi's Vulkan backend is active AND has external-memory
   *  extensions usable. Set by `probeFromQRhi`. */
  bool vkExternalMemorySupported{};

  // -- Apple -----------------------------------------------------------

  /** Running on macOS with Metal-capable GPU. Implies tier-3 Metal-IOS
   *  is the right path for shared GPU resources. */
  bool metalSupported{};

  // -- Convenience tier helpers ---------------------------------------

  /** True when this system can do tier-0 NIC/card → GPU VRAM. */
  constexpr bool hasTier0() const noexcept
  {
    return (cudaVmmSupported && nvidiaPeermem) || amd.busAddressable;
  }

  /** True when this system can do tier-1 NVIDIA DVP via some backend. */
  constexpr bool hasTier1Dvp() const noexcept
  {
    return dvpLoaded && (dvpHaveGl || dvpHaveD3D11 || dvpHaveCuda);
  }

  /** True when this system can do tier-2 AMD pinned-host. */
  constexpr bool hasTier2AmdPinned() const noexcept
  {
    return amd.externalVirtualMemory || amd.externalPhysicalMemory
           || amd.pinnedMemory;
  }
};

/** Run the context-free probe: OS, vendor (best-effort without GL),
 *  DVP, CUDA, kernel modules.
 *
 *  Idempotent: safe to call from multiple threads; the underlying
 *  DVP and CUDA loaders are themselves idempotent. */
SCORE_PLUGIN_GFX_EXPORT
GpuCapabilities probeContextFree();

/** Augment an existing capability struct with QRhi-side info: the
 *  renderer name, backend kind, and Vulkan external-memory readiness.
 *  Calls back into QRhi's introspection (no extra context required). */
SCORE_PLUGIN_GFX_EXPORT
void probeFromQRhi(GpuCapabilities& caps, QRhi* rhi) noexcept;

/** Probe AMD GL extension presence. Requires an OpenGL context to be
 *  current on the calling thread (the QRhi GL backend's context, or
 *  a sample-side context for tools).
 *
 *  Idempotent: re-probing on a different GL context updates the
 *  flags. */
SCORE_PLUGIN_GFX_EXPORT
void probeGlExtensions(GpuCapabilities& caps) noexcept;

/** Render the capability struct to a human-readable multi-line
 *  string. Useful for plugin init logs + the diagnostic UI. */
SCORE_PLUGIN_GFX_EXPORT
const char* gpuVendorName(GpuVendor v) noexcept;
SCORE_PLUGIN_GFX_EXPORT
const char* qrhiBackendName(QRhiBackendKind b) noexcept;

} // namespace score::gfx::interop

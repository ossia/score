#pragma once

/**
 * @file AmdPinnedBuffers.hpp
 * @brief AMD GL pinned-memory and bus-addressable extension wrapper.
 *
 * AMD's GL driver exposes three extensions that score's vendor-neutral
 * `HostPinnedRing` and (where supported) a direct P2P backend can use:
 *
 *   - `GL_AMD_pinned_memory` — bind a host pointer as the storage of a
 *     GL buffer via `glBufferData(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD,
 *     size, host_ptr, usage)`. The GL driver page-locks the host memory
 *     and DMA-reads/writes it directly. This is the path DeckLink's
 *     `LoopThroughWithOpenGLCompositing` sample uses.
 *
 *   - `GL_AMD_bus_addressable_memory` — true P2P. Allocate a buffer in
 *     visible GPU framebuffer (`GL_BUS_ADDRESSABLE_MEMORY_AMD`), call
 *     `glMakeBuffersResidentAMD` to retrieve a bus address that a
 *     third-party DMA engine (capture card, NIC) can write into
 *     directly. AJA's `demos/AMD/SDICommon/GLTransferBuffers.cpp`
 *     exercises this for direct P2P SDI capture/output.
 *
 *   - `GL_EXTERNAL_VIRTUAL_MEMORY_AMD` / `GL_EXTERNAL_PHYSICAL_MEMORY_AMD`
 *     — older variants of the same idea; the bridge probes for them
 *     and exposes their presence so the consumer can pick the most-
 *     specific path that's present.
 *
 * Mechanism: this is a pure GL-extension wrapper. It depends on a live
 * GL context being current on the calling thread. It resolves entry
 * points via Qt's `QOpenGLContext::getProcAddress` so there's no
 * link-time dependency on libGL or libGLEW.
 *
 * The bridge is owned by the GL backend of `HostPinnedRing` (and the
 * planned AMD direct-P2P strategy); other backends — Vulkan, D3D11, Metal
 * — won't instantiate it.
 */

#include <score_plugin_gfx_export.h>

#include <cstddef>
#include <cstdint>

class QOpenGLContext;

namespace score::gfx::interop
{

/** AMD GL extension token values, exposed without pulling in glext.h.
 *  Numbers come from the AMD extension registry (registered enums). */
namespace amd_gl_tokens
{
constexpr unsigned int EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD = 0x9160;
constexpr unsigned int BUS_ADDRESSABLE_MEMORY_AMD = 0x9168;
constexpr unsigned int EXTERNAL_VIRTUAL_MEMORY_AMD = 0x9161;
constexpr unsigned int EXTERNAL_PHYSICAL_MEMORY_AMD = 0x9162;
} // namespace amd_gl_tokens

/** Per-extension presence and resolved entry points. */
struct SCORE_PLUGIN_GFX_EXPORT AmdPinnedBuffers
{
  // -- Presence flags --------------------------------------------------

  /** GL_AMD_pinned_memory — DeckLink-style sysmem→GL fast path. */
  bool hasPinnedMemory{};
  /** GL_AMD_bus_addressable_memory — direct P2P (AJA-AMD path). */
  bool hasBusAddressable{};
  /** Older AJA-AMD virtual-memory token. Treated as a fallback for
   *  pinnedMemory on driver builds that ship the older name only. */
  bool hasExternalVirtualMemory{};
  /** Older AJA-AMD physical-memory token. Used for the pack-buffer
   *  read-back path on the bus-addressable variant. */
  bool hasExternalPhysicalMemory{};

  // -- Resolved entry points (bus-addressable only) -------------------

  /** glMakeBuffersResidentAMD(GLsizei n, const GLuint* buffers,
   *                            GLuint64* outBusAddr, GLuint64* outMarkerAddr) */
  using FN_MakeBuffersResident
      = void (*)(int /*n*/, const unsigned int* /*buffers*/,
                 uint64_t* /*busAddr*/, uint64_t* /*markerAddr*/);
  FN_MakeBuffersResident makeBuffersResident{};

  /** glBufferBusAddressAMD(GLenum target, GLsizeiptr size,
   *                        GLuint64 busAddr, GLuint64 markerAddr) */
  using FN_BufferBusAddress
      = void (*)(unsigned int /*target*/, std::ptrdiff_t /*size*/,
                 uint64_t /*busAddr*/, uint64_t /*markerAddr*/);
  FN_BufferBusAddress bufferBusAddress{};

  // -- API ------------------------------------------------------------

  /** Probe the given QOpenGLContext for AMD extension presence and
   *  resolve entry points. Re-callable; idempotent on the same
   *  context. Returns true iff at least one of the three pinned-host
   *  variants is usable.
   *
   *  Pass nullptr to attempt detection via `QOpenGLContext::currentContext()`. */
  bool tryInit(QOpenGLContext* ctx = nullptr) noexcept;

  /** True when any AMD pinned-host or bus-addressable mechanism is
   *  available. Useful as the top-level "should the HostPinnedRing
   *  pick the AMD backend?" gate. */
  bool any() const noexcept
  {
    return hasPinnedMemory || hasBusAddressable || hasExternalVirtualMemory
           || hasExternalPhysicalMemory;
  }

  /** Create a GL buffer whose storage is the supplied host pointer
   *  (the GL_AMD_pinned_memory path, with fallback to the older
   *  GL_EXTERNAL_VIRTUAL_MEMORY_AMD token). The buffer is bound to
   *  `target` (e.g. `GL_PIXEL_UNPACK_BUFFER` for input,
   *  `GL_PIXEL_PACK_BUFFER` for output), filled with `host_ptr`, then
   *  unbound from `target` — the caller re-binds as needed.
   *
   *  `host_ptr` MUST be at least 4 KiB aligned (use
   *  `nv_dvp_aligned_alloc` from the DVP bridge or a posix_memalign).
   *  The buffer takes a non-owning reference to `host_ptr`; the
   *  caller keeps it alive for the buffer's lifetime.
   *
   *  Returns the new GL buffer ID, or 0 on failure. */
  unsigned int createPinnedBuffer(
      unsigned int target, std::size_t size_bytes, void* host_ptr,
      unsigned int usage) noexcept;

  /** Create a GL buffer in visible GPU framebuffer that's writable by
   *  third-party DMA engines via its bus address. Direct P2P path.
   *
   *  On success, fills `out_busAddr` and `out_markerAddr` — the bus
   *  address is what the consumer hands to the capture card / NIC;
   *  the marker is used by the device to signal completion (it must
   *  be written to a known value when the DMA finishes).
   *
   *  Only valid when `hasBusAddressable == true`. */
  unsigned int createBusAddressableBuffer(
      std::size_t size_bytes, unsigned int usage, uint64_t* out_busAddr,
      uint64_t* out_markerAddr) noexcept;
};

} // namespace score::gfx::interop

#pragma once

/**
 * @file HostPinnedRing.hpp
 * @brief Vendor-neutral N-slot sysmem ring for tier-1/2/4 transfers.
 *
 * Counterpart to `ImportedGpuBufferRing` (which holds tier-0 GPU-side buffers
 * for true P2P). `HostPinnedRing` holds host-side page-locked buffers
 * that a video card's DMA engine writes into, and a per-backend
 * transfer primitive that moves slot contents to/from a QRhi texture.
 *
 * The class is consumed by every vendor strategy that goes through
 * host RAM on the hot path: DeckLink (all variants), AJA tier-1 DVP,
 * Magewell, Datapath, Osprey, USB capture, GenICam, etc. The vendor
 * supplies the page-locked buffer (via its `DMABufferLock`,
 * `MWPinVideoBuffer`, etc) and the slot index it just filled; the
 * ring runs the QRhi-side ingest via the configured backend.
 *
 * Backend selection is automatic:
 *
 *   1. DVP (NVIDIA Quadro/Tesla, Win+Linux) — fastest sysmem→texture
 *      mechanism on NVIDIA HW. Uses `nv_dvp_copy_buffer_to_texture`.
 *   2. AMD-pinned (AMD, Win+Linux) — `GL_AMD_pinned_memory`-class
 *      buffer bound as GL_PIXEL_UNPACK_BUFFER + `glTexSubImage2D`.
 *      Only when QRhi is on the OpenGL backend.
 *   3. cudaHostRegister (NVIDIA, any backend) — page-locks the slot
 *      then `cuMemcpy2D` into a CUDA-imported QRhi texture. Fallback
 *      when DVP isn't loaded but CUDA is.
 *   4. CPU staging — plain QRhi `uploadTexture` from the slot's host
 *      pointer. Always available.
 *
 * The picker walks the list in order, choosing the first backend
 * compatible with the live QRhi backend + the `GpuCapabilities`
 * probe.
 */

#include <Gfx/Graph/interop/GpuCapabilities.hpp>
#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <score_plugin_gfx_export.h>

#include <cstddef>
#include <cstdint>
#include <memory>

class QRhi;
class QRhiTexture;
class QRhiResourceUpdateBatch;

namespace score::gfx::interop
{

/** Which backend a `HostPinnedRing` instance is using. Set by
 *  `create()` and queryable via `backend()`. */
enum class HostPinnedRingBackend : uint8_t
{
  None = 0,
  Dvp,             /**< NVIDIA DVP — fastest sysmem path. */
  AmdPinned,       /**< AMD GL pinned-memory or virtual-memory. */
  CudaHostReg,     /**< cudaHostRegister + cuMemcpy2D. */
  CpuStaging,      /**< QRhi uploadTexture / readbackTexture fallback. */
};

/** Direction of the per-slot transfer. */
enum class HostPinnedDirection : uint8_t
{
  CaptureToTexture = 0, /**< Vendor wrote into slot, ring uploads to texture. */
  TextureToBuffer,      /**< Ring downloads texture to slot, vendor reads. */
};

/** One slot of the ring. The host buffer is page-locked (4 KiB
 *  aligned, allocated via `nv_dvp_aligned_alloc`). Backends may
 *  attach additional per-slot resources (DVP buffer handle, GL
 *  buffer id, etc) which live in the opaque pointer below. */
struct SCORE_PLUGIN_GFX_EXPORT HostPinnedSlot
{
  void* host{};         /**< Page-locked sysmem; 4 KiB aligned. */
  std::size_t size{};   /**< Slot byte size; stride * height. */
  std::size_t stride{}; /**< Bytes per scanline. */
  void* backendOpaque{}; /**< Owned by the backend; nullptr for CPU. */
};

struct HostPinnedRingConfig
{
  QRhi* rhi{};
  const GpuCapabilities* caps{}; /**< Borrowed; must outlive the ring. */
  HostPinnedDirection direction{HostPinnedDirection::CaptureToTexture};
  /** Wire format the ring carries (the shared neutral enum). Only the packed
   *  subset is meaningful here — stride comes from interop::defaultStride. */
  VideoPixelFormat format{VideoPixelFormat::BGRA8};
  uint32_t width{};
  uint32_t height{};
  uint32_t stride{0}; /**< 0 = compute from format + width with default padding. */
  uint32_t slotCount{2};
  const char* debugName{"score-host-pinned-ring"};
};

/** Vendor-neutral host-pinned ring. Lifecycle:
 *
 *   1. `create(cfg)` allocates `slotCount` page-locked buffers and
 *      sets up the chosen backend.
 *   2. The vendor strategy hands `slot(i).host` to its DMA-pin API
 *      (`DMABufferLock`, `MWPinVideoBuffer`, custom `IDeckLinkVideoBuffer`).
 *   3. Per frame (capture):
 *        - vendor calls back with slotIdx i; ring runs
 *          `uploadSlotToTexture(i, dstTex)`.
 *   4. Per frame (output):
 *        - ring runs `downloadTextureToSlot(i, srcTex)`; vendor schedules
 *          slot i for transmission.
 *   5. `destroy()` tears down backend resources + frees host buffers.
 */
class SCORE_PLUGIN_GFX_EXPORT HostPinnedRing
{
public:
  HostPinnedRing() noexcept;
  ~HostPinnedRing();

  HostPinnedRing(const HostPinnedRing&) = delete;
  HostPinnedRing& operator=(const HostPinnedRing&) = delete;
  HostPinnedRing(HostPinnedRing&&) noexcept;
  HostPinnedRing& operator=(HostPinnedRing&&) noexcept;

  /** Allocate slots + pick a backend.
   *
   *  @return  true on success. On failure partial state is released
   *           before returning. */
  bool create(const HostPinnedRingConfig& cfg);

  /** Release all slots + backend resources. Safe to call multiple
   *  times; idempotent.
   *
   *  **Teardown contract for async backends (CPU / CudaHostReg)**:
   *  the caller must drain pending readbacks before calling destroy().
   *  Drain by polling `slotReady(i)` for every slot that had an
   *  outstanding `downloadTextureToSlot` call, or by submitting one
   *  or two more dummy frames after stopping new downloads so QRhi
   *  flushes its queue. Destroying while a readback is pending is UB —
   *  QRhi holds a raw pointer to the per-slot `QRhiReadbackResult` and
   *  will write into freed memory if the result is destroyed first.
   *  The implementation logs a warning if destroy() sees a pending
   *  flag, but the UB has already been incurred at that point. */
  void destroy() noexcept;

  bool valid() const noexcept;
  HostPinnedRingBackend backend() const noexcept;
  std::size_t slotCount() const noexcept;

  /** Slot accessors. Index must be < slotCount(). */
  HostPinnedSlot& slot(std::size_t i) noexcept;
  const HostPinnedSlot& slot(std::size_t i) const noexcept;

  /** Bind a CUDA-imported destination handle for the CudaHostReg
   *  backend's zero-copy upload path.
   *
   *  When set, `uploadSlotToTexture` for that backend issues a
   *  `cuMemcpy2DAsync` from the slot's page-locked host pointer
   *  (registered via `cuMemHostRegister(DEVICEMAP)`) directly to the
   *  CUarray — bypassing the QRhi staging upload. The caller is
   *  expected to:
   *    1. Allocate the destination QRhiTexture as an exportable Vulkan
   *       image via `vkinterop::createExportableImage`.
   *    2. Import it into CUDA via `cuda_interop_import_vulkan_image`
   *       (returns a CUarray for the level-0 face).
   *    3. Call `bindCudaDestination(dst, cuda_interop_ctx, cuda_array)` once.
   *    4. Use the same QRhiTexture pointer in each
   *       `uploadSlotToTexture(i, dst, ...)` call.
   *
   *  `cuda_array` is a `CUarray` cast to `void*` to avoid pulling
   *  CudaFunctions.hpp into score's gfx public headers. `cuda_ctx`
   *  is the `CudaInteropContextHandle` (also `void*`).
   *
   *  Calling with `cuda_array = nullptr` clears the binding for that
   *  texture (upload falls back to the CPU staging path).
   *
   *  Without an explicit binding, CudaHostReg upload routes through
   *  the same CPU staging as the CpuStaging backend — page-locking
   *  the host buffer is then perf-neutral (the QRhi upload doesn't
   *  see the pinning). The full zero-copy benefit only kicks in
   *  with the binding above. */
  void bindCudaDestination(
      QRhiTexture* dst, void* cuda_ctx, void* cuda_array) noexcept;

  /** Upload slot `i`'s host bytes to `dst`. The destination texture
   *  must match the ring's format + dimensions.
   *
   *  For the **CPU** backend the call appends a `QRhiTextureUploadEntry`
   *  to `batch`; the actual GPU work happens when the caller commits
   *  the batch via the active frame submit.
   *
   *  For **DVP** / **AMD-pinned** / **CudaHostRegister** backends
   *  the call runs the vendor-side DMA synchronously and `batch` is
   *  ignored (may be nullptr). The texture is consistent on return.
   *
   *  This signature keeps the CPU fast-path zero-allocation while
   *  still working for backends that need their own sync. */
  bool uploadSlotToTexture(std::size_t i, QRhiTexture* dst,
                           QRhiResourceUpdateBatch* batch = nullptr);

  /** Read `src` into slot `i`'s host bytes.
   *
   *  For the **CPU** and **CudaHostRegister** backends the call appends
   *  a `readBackTexture` request to `batch`. The readback is async:
   *  bytes land in `slot(i).host` only after the GPU completes the
   *  enqueued frame and QRhi fires the completion callback (typically
   *  one or two frames later). Poll readiness with `slotReady(i)` and
   *  call `resetSlotReady(i)` once the vendor strategy has consumed
   *  the slot.
   *
   *  For **DVP** / **AMD-pinned** backends the call runs the vendor
   *  DMA synchronously; the slot is ready on return and `slotReady`
   *  immediately reports true. */
  bool downloadTextureToSlot(QRhiTexture* src, std::size_t i,
                             QRhiResourceUpdateBatch* batch = nullptr);

  /** True when the most recent `downloadTextureToSlot` for slot `i`
   *  has completed and `slot(i).host` contains valid bytes. Async
   *  backends (CPU/CudaHostReg) toggle this from the QRhi completion
   *  callback; sync backends set it inside the download call itself. */
  bool slotReady(std::size_t i) const noexcept;

  /** Clear the ready flag for slot `i`. Call after the vendor strategy
   *  has consumed `slot(i).host` and before enqueueing the next
   *  download for the same slot. Failure to reset doesn't corrupt
   *  state — it just makes the next ready-poll return true immediately. */
  void resetSlotReady(std::size_t i) noexcept;

  /** Current write index. Caller-driven; the ring doesn't auto-rotate
   *  — vendor decides which slot is "next" based on its own pacing. */
  std::size_t writeIndex() const noexcept;
  std::size_t advance() noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace score::gfx::interop

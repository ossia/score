#pragma once
/**
 * @file HostStagedOutput.hpp
 * @brief Vendor-neutral host-staged video OUTPUT helper.
 *
 * The CPU-staging counterpart to GpuDirectOutput, and the output-direction
 * counterpart to HostPinnedRing: the path used whenever GPU-direct DMA isn't
 * available (no Quadro/DVP, no RDMA driver, AMD, etc) - which is the common
 * case on consumer GPUs. It owns the whole "encode on GPU -> read back ->
 * hand a host pointer to the card's DMA" pipeline so each vendor addon only
 * supplies its pin/unpin callbacks and its per-frame submit/pacing.
 *
 * What the helper owns:
 *   - The two double-buffered GPUVideoEncoders (RGBA -> wire-format bytes).
 *   - A page-locked staging ring (vendor-pinned via VendorDmaRegistrar).
 *   - The direct-DMA-from-readback fast path: when a single-plane readback is
 *     already byte-identical to the whole framestore (tight pitch == card
 *     pitch, full height), it page-locks the encoder's readback buffer and
 *     returns it directly, skipping the per-frame full-frame memcpy. Falls
 *     back to the ring copy for multi-plane / padded-pitch / custom-staged.
 *
 * What the vendor adapter provides:
 *   - VendorDmaRegistrar: page-lock (e.g. AJA DMABufferLock, Magewell
 *     MWPinVideoBuffer) for the ring buffers and the readback buffers.
 *   - HostStagedPlane descriptors (row bytes + total raster bytes per plane),
 *     derived from the card's format descriptor.
 *   - Optional customStage callback for vendor-specific single-plane packing
 *     (e.g. AJA's UYVY->v210 CPU pack for non-mod-6 widths).
 *   - Per-frame, after prepareNextFrame() returns a host pointer: submit it to
 *     the card's DMA API and pace it (AutoCirculateTransfer, DeckLink
 *     ScheduleVideoFrame, ...). Pacing stays vendor-specific.
 *
 * Threading: init / encodeFrame / prepareNextFrame / release run on the render
 * thread (QRhi affinity). The returned host pointer stays valid until the
 * buffer is reused (ring: kSlots frames later; direct: 2 frames later) - long
 * enough for a synchronous DMA submit on the vendor's pump thread.
 */
#include <score_plugin_gfx_export.h>

#include <Gfx/Graph/interop/VendorDmaRegistrar.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

class QRhi;
class QRhiCommandBuffer;
class QRhiTexture;

namespace score::gfx
{
struct RenderState;
struct GPUVideoEncoder;
}

namespace score::gfx::interop
{

/// Per-plane destination geometry, from the card's format descriptor.
struct HostStagedPlane
{
  int rowBytes{};               ///< destination bytes per row (card pitch)
  std::uint32_t rasterBytes{};  ///< total bytes for this plane in the framestore
};

struct HostStagedOutputConfig
{
  QRhi* rhi{};
  const score::gfx::RenderState* state{};
  int width{};
  int height{};
  std::uint32_t frameByteSize{};   ///< total framestore bytes (all planes)
  int visibleRows{};               ///< visible raster height
  int slotCount{4};                ///< staging ring depth
  std::vector<HostStagedPlane> planes;  ///< 1 entry = single-plane, N = planar
  VendorDmaRegistrar registrar;    ///< page-lock callbacks (both required)
  bool directDmaEnabled{true};

  /// Optional vendor-specific single-plane staging (return true if fully
  /// handled, false to fall back to the default row-stride copy). Used for
  /// e.g. AJA's UYVY->v210 CPU pack when the GPU encoder can't (width % 6).
  std::function<bool(
      const std::uint8_t* src, int srcRowBytes, std::uint8_t* dst,
      int dstRowBytes, int rows)>
      customStage;
};

/**
 * @brief Generic host-staged output. Vendor-neutral.
 */
class SCORE_PLUGIN_GFX_EXPORT HostStagedOutput
{
public:
  HostStagedOutput();
  ~HostStagedOutput();

  HostStagedOutput(const HostStagedOutput&) = delete;
  HostStagedOutput& operator=(const HostStagedOutput&) = delete;

  /**
   * @brief Allocate + pin the staging ring and take ownership of the two
   *        already-init()'d double-buffered encoders.
   *
   * The encoders must already have been init()'d by the caller against the
   * source texture (so the vendor controls encoder selection + colour
   * conversion). Returns false if the ring allocation or a vendor pin fails;
   * partial state is released before returning.
   */
  bool init(
      HostStagedOutputConfig cfg,
      std::unique_ptr<score::gfx::GPUVideoEncoder> enc0,
      std::unique_ptr<score::gfx::GPUVideoEncoder> enc1);

  bool valid() const noexcept;

  /// Inside the offscreen frame, after the scene rendered into the source
  /// texture: run the current encoder (conversion pass + schedule readback).
  void encodeFrame(QRhiCommandBuffer& cb);

  /// After endOffscreenFrame(): stage the just-read-back frame (direct-DMA or
  /// ring copy) and return the host pointer the vendor should DMA from, then
  /// advance the double-buffer. Returns nullptr if no frame is ready.
  void* prepareNextFrame();

  void release();

private:
  struct Slot;
  std::unique_ptr<Slot> m_state;
};

} // namespace score::gfx::interop

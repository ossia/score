#pragma once

/**
 * @file DirectVideoOutputBackend.hpp
 * @brief Vendor seam for professional capture-card OUTPUT (playout).
 *
 * The output-side counterpart to `DMACaptureBackend`. A vendor (AJA, and future
 * DeckLink/Bluefish/Deltacast) implements this; the shared
 * `DirectVideoOutputNode` (a score::gfx::OutputNode) owns the QRhi + render loop
 * and composes the already-generic pieces around it:
 *
 *   encoders  : makeWireEncoder(backend->wireFormat())
 *   GPU-direct: selectVideoOutputStrategy(cfg, backend->gpuDirectCandidates(...))
 *   host path : CpuStagedVideoOutput(backend->planes/registrar/customStage)
 *   pacing    : PacedFramePump(backend->pacingHooks())
 *
 * So a vendor playout addon shrinks to: open the device + set the video
 * standard/link/routing/VPID/HDR, report geometry + the neutral wire format,
 * supply a pin adapter (VendorDmaRegistrar), the GPU-direct strategy candidates
 * for the active graphics API, and the pacing hooks (wait-for-tick + submit).
 *
 * This is the generalisation of what AJANode::createOutput does inline today.
 */

#include <Gfx/Graph/RenderState.hpp> // GraphicsApi
#include <Gfx/Graph/interop/VideoOutputStrategy.hpp>
#include <Gfx/Graph/interop/CpuStagedVideoOutput.hpp> // HostStagedPlane
#include <Gfx/Graph/interop/PacedFramePump.hpp>
#include <Gfx/Graph/interop/VendorDmaRegistrar.hpp>
#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <score_plugin_gfx_export.h>

#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

class QRhi;
class QRhiTexture;

namespace score::gfx
{

/**
 * @brief Vendor playout backend. Constructed with its vendor-specific settings
 *        (video format, pixel format, channel, link, HDR, ...); `open()` then
 *        brings the device up. The node queries geometry/format/planes and
 *        drives playout through the registrar + GPU-direct candidates + pacing
 *        hooks.
 */
struct SCORE_PLUGIN_GFX_EXPORT DirectVideoOutputBackend
{
  /// CPU-pack hook signature (v210 non-mod-6, 2SI->SQD, byte-swap), matching
  /// CpuStagedVideoOutputConfig::customStage.
  using CustomStage = std::function<bool(
      const std::uint8_t* src, int srcRowBytes, std::uint8_t* dst,
      int dstRowBytes, int rows)>;

  virtual ~DirectVideoOutputBackend();

  /// Open the device + set the video standard, link/routing, VPID and HDR ANC.
  /// Called before the QRhi exists (the node sizes the QRhi to the negotiated
  /// geometry), so no QRhi here — `api` is for vendors that pick a topology by
  /// backend. Returns false on failure. Geometry/format/planes valid only after
  /// a true return.
  virtual bool open(GraphicsApi api) = 0;

  /// Negotiated raster geometry (the QRhi scene texture is sized to this).
  virtual int width() const noexcept = 0;
  virtual int height() const noexcept = 0;

  /// Frame rate (Hz) — drives the node's manual rendering rate.
  virtual double frameRate() const noexcept = 0;

  /// True once open() has succeeded (drives canRender()).
  virtual bool isOpen() const noexcept = 0;

  /// The on-wire pixel format the card framestore expects (-> makeWireEncoder).
  virtual interop::VideoPixelFormat wireFormat() const noexcept = 0;

  /// The pixel format the GPU encoder should *produce*. Equals wireFormat()
  /// unless a CPU repack bridges them (e.g. AJA v210 at non-mod-6 widths emits
  /// UYVY on the GPU and packs to v210 in customStage). Default: wireFormat().
  virtual interop::VideoPixelFormat encoderFormat() const noexcept
  {
    return wireFormat();
  }

  /// Render the scene into an RGBA16F intermediate (true) instead of RGBA8, for
  /// >8-bit / HDR wire formats so the encoder sees real precision.
  virtual bool prefersFloatRender() const noexcept { return false; }

  /// The RGB->wire colour-conversion shader fragment (e.g. score::gfx::
  /// colorMatrixOut(...)) for the active colour space / HDR mode.
  virtual QString colorConversion() const = 0;

  /// Total framestore byte size (accounts for stride padding) and the number of
  /// visible rows to copy; plus the per-plane geometry for CpuStagedVideoOutput.
  virtual std::uint32_t frameByteSize() const noexcept = 0;
  virtual int visibleRows() const noexcept = 0;
  virtual std::vector<interop::HostStagedPlane> planes() const = 0;

  /// Pin/unpin adapter for the staging ring (DMABufferLock / VirtualLock /
  /// bfAlloc-noop / VHD_CreateSlotEx). May be empty (no pinning).
  virtual interop::VendorDmaRegistrar registrar() = 0;

  /// Optional CPU pack applied before DMA (v210 non-mod-6, UHD 2SI->SQD, byte
  /// swap). Empty = plain row-stride copy.
  virtual CustomStage customStage() { return {}; }

  /// Prefer a GPU-direct (DVP) download in the host-staged path: the node lets
  /// CpuStagedVideoOutput DMA the encoder texture straight to a vendor-registered
  /// sysmem ring (skipping the QRhi readback) when a GPU-direct backend exists,
  /// falling back to CPU readback otherwise. Default false (AJA uses its own
  /// gpuDirectCandidates strategy instead). DeckLink opts in.
  virtual bool prefersGpuDownload() const noexcept { return false; }

  /// GPU-direct output strategy candidates for the active graphics API, in
  /// priority order (DVP before zero-copy RDMA, etc). Empty => host-staged only.
  /// The node feeds these to selectVideoOutputStrategy().
  virtual std::vector<std::function<std::unique_ptr<interop::VideoOutputStrategy>()>>
  gpuDirectCandidates(QRhi* rhi, GraphicsApi api) = 0;

  /// Clock-paced submit hooks (wait on the output VBI / schedule-callback,
  /// card back-pressure, and the actual transfer of a frame pointer to the
  /// card). The node owns the PacedFramePump built from these.
  virtual interop::PacedFramePump::Hooks pacingHooks() = 0;

  /// Optional external-genlock tick source (opt-in): a blocking wait
  /// that returns true on the next hardware output tick (card VBI), false on
  /// timeout. Empty (default) => no hardware genlock available; the node keeps
  /// its timer clock. Consumed by ExternalGenlockClock to phase-lock render to
  /// the card. NOTE: this is generally a SECOND waiter on the same output VBI
  /// as pacingHooks().waitForTick — safe where the VBI is a broadcast
  /// wait-queue (Linux AJA); validate for auto-reset-event platforms.
  virtual std::function<bool()> genlockTickSource() { return {}; }

  /// Stop the card from reading ANY application buffer, synchronously.
  ///
  /// Called by DirectVideoOutputNode::destroyOutput() after the pump thread
  /// has stopped but BEFORE the GPU-direct strategy and host-staged ring are
  /// released. Backends whose submit path hands buffers to an asynchronous /
  /// retained queue (DeckLink ScheduleVideoFrame, Deltacast RDMA application-
  /// buffer slots) must stop streaming here and wait until the hardware has
  /// let go of every in-flight buffer — otherwise the node frees memory the
  /// card is still DMA-reading. Synchronous-submit vendors (AJA DMAWriteFrame)
  /// keep the default no-op. close() still runs afterwards and must tolerate
  /// quiesce() having already stopped the stream.
  virtual void quiesce() { }

  /// Tear down the device (stop AutoCirculate/streams, restore task mode, close).
  virtual void close() = 0;
};

} // namespace score::gfx

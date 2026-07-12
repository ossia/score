#pragma once

/**
 * @file VideoOutputStrategy.hpp
 * @brief Vendor-neutral interface for GPU-direct video OUTPUT strategies.
 *
 * Generalises the AJA `RdmaInterop` interface so any peer device that
 * supports "give me a GPU pointer I can DMA from" (AJA SDI cards via
 * `DMABufferLock(inRDMA=true)`, Magewell ProCapture via NVRDMA-mode
 * `MWPinVideoBuffer`, NVIDIA Rivermax via `rmx_register_memory` + GPU
 * payload, NVIDIA DVP for AJA on Windows, future Bluefish/Deltacast/
 * Datapath variants) can hang strategies off the same dispatch chain
 * inside its addon.
 *
 * Concrete implementations live in each vendor's addon
 * (`score-addon-aja/AJA/RdmaInterop*Tier3.hpp` etc); only the *interface*
 * and the supporting helpers (`ImportedGpuBufferRing`, `InteropFence`,
 * `ComputeRingDispatcher`, `RdmaVideoOutput`) are in score-plugin-gfx so
 * the per-vendor file shrinks to ~50 lines of "construct a
 * `RdmaVideoOutput`, fill in the pin/unpin callbacks".
 *
 * Lifecycle:
 *   1. Vendor addon picks a strategy based on backend + feature
 *      availability + vendor-specific format constraints; constructs it
 *      with the vendor-specific bits (card handle, format enum, channel,
 *      ...) as constructor args.
 *   2. Addon calls `init(VideoOutputStrategyConfig{rhi, state, sourceTex,
 *      w, h, frameByteSize})`. On failure (no CUDA, format unsupported,
 *      vendor refused to pin), strategy releases its partial state and
 *      returns false; addon falls back to the next strategy (or CPU
 *      staging).
 *   3. Per frame:
 *        - `encodeFrame(cb)` inside the offscreen frame.
 *        - `prepareNextFrame()` after `endOffscreenFrame` returns the
 *          GPU pointer to feed the vendor submission API
 *          (`AutoCirculateTransfer.SetVideoBuffer`,
 *          `MWCaptureVideoFrameToPhysicalAddress`,
 *          `rmx_output_*_send_chunk`, ...).
 *   4. `release()` at shutdown.
 */

#include <cstdint>

class QRhi;
class QRhiTexture;
class QRhiCommandBuffer;

namespace score::gfx
{
struct RenderState;
}

namespace score::gfx::interop
{

/**
 * @brief Generic config passed to `VideoOutputStrategy::init`. Vendor-
 *        specific bits (card handle, pixel-format enum, ...) live on
 *        the strategy class itself.
 */
struct VideoOutputStrategyConfig
{
  QRhi* rhi{};

  /// Render state, needed by strategies that compile compute shaders
  /// (`ComputeEncoder::init` requires it for shader-version negotiation).
  /// Nullable when a strategy doesn't need it.
  const score::gfx::RenderState* state{};

  /// The RGBA scene texture the input pipeline renders into. Strategies
  /// either compute-convert this into the vendor pixel format or blit
  /// it through native interop.
  QRhiTexture* sourceTexture{};

  int width{};
  int height{};

  /// Frame size in vendor pixel format bytes (already accounts for row
  /// stride padding, e.g. v210 line layout). Strategies allocate the
  /// buffer ring at this byte size and the vendor pin callback receives
  /// it directly.
  std::uint32_t frameByteSize{};
};

/**
 * @brief Per-graphics-API strategy interface for vendor GPU-direct
 *        output. Each addon implements one impl per (graphics API,
 *        vendor interop family) tuple.
 */
struct VideoOutputStrategy
{
  virtual ~VideoOutputStrategy() = default;

  /// Short human-readable identifier used in logs, e.g. "DVP-D3D11" or
  /// "RDMA-D3D11/T3" / "Magewell-NVRDMA/GL". Dispatch sites print this
  /// when trying each candidate so the log shows which one engaged.
  virtual const char* name() const noexcept = 0;

  virtual bool init(const VideoOutputStrategyConfig& cfg) = 0;
  virtual void release() = 0;

  /// Inside the offscreen frame, after the input pipeline rendered into
  /// `sourceTexture`: dispatch the compute / blit pass that produces
  /// the vendor pixel format. D3D11 / DVP strategies may leave this
  /// empty if their conversion happens post-endFrame on the immediate
  /// context.
  virtual void encodeFrame(QRhiCommandBuffer& cb) = 0;

  /// After `endOffscreenFrame`: any cross-API sync (D3D12 fence,
  /// Vulkan timeline, glFinish) and ring-slot rotation. Returns the
  /// GPU pointer to feed the vendor's per-frame submission API, or
  /// nullptr if the frame can't be transferred (caller drops).
  virtual void* prepareNextFrame() = 0;
};

} // namespace score::gfx::interop

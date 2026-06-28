#pragma once

/**
 * @file GpuDirectOutput.hpp
 * @brief Vendor-neutral GPU-direct output strategy.
 *
 * Generalises the AJA tier-3 RDMA output path so any peer device with a
 * "pin GPU memory for P2P DMA" call (AJA `DMABufferLock(inRDMA=true)`,
 * Magewell `MWPinVideoBuffer`, Rivermax `rmx_register_memory`, ...) can
 * reuse the same buffer-ring + encoder + fence plumbing.
 *
 * What the helper owns:
 *   - A `CudaP2PContextHandle` (init at create, destroy at release).
 *   - A `GpuRingBuffer` of N CUDA-imported QRhi storage buffers.
 *   - A per-backend `InteropFence` for cross-API ordering.
 *   - A `ComputeRingDispatcher` that runs the per-slot encoder.
 *
 * What the vendor adapter provides via `VendorDmaRegistrar`:
 *   - A `register(gpuPtr, size)` callback the helper invokes once per
 *     slot at init time. Typical impl: `card->DMABufferLock(...)`.
 *   - A `release(gpuPtr, size)` callback invoked at shutdown.
 *
 * Per-frame, the vendor calls:
 *   - `encodeFrame(cb)` inside the offscreen frame.
 *   - `prepareNextFrame()` after `endOffscreenFrame` to wait on the
 *     fence and get the GPU pointer the peer should DMA from. The
 *     adapter passes that pointer to its vendor submission API
 *     (`AutoCirculateTransfer.SetVideoBuffer`, `MWCaptureVideoFrame*`,
 *     `rmx_output_*_send_chunk`, ...) and then submits the frame.
 *
 * Pacing stays vendor-specific (AJA VBI wait, Magewell event, Rivermax
 * PTP). This helper is purely the GPU-side plumbing.
 */

#include <Gfx/Graph/encoders/ComputeEncoder.hpp>
#include <Gfx/Graph/interop/ComputeRingDispatcher.hpp>
#include <Gfx/Graph/interop/CudaP2PBridge.h>
#include <Gfx/Graph/interop/GpuRingBuffer.hpp>
#include <Gfx/Graph/interop/InteropFence.hpp>
#include <Gfx/Graph/interop/VendorDmaRegistrar.hpp>
#include <score_plugin_gfx_export.h>

#include <cstdint>
#include <functional>
#include <memory>

class QRhi;
class QRhiCommandBuffer;
class QRhiTexture;

namespace score::gfx
{
struct RenderState;
}

namespace score::gfx::interop
{

// VendorDmaRegistrar (the per-slot pin/unpin hooks supplied by the vendor
// adapter) now lives in VendorDmaRegistrar.hpp, shared with HostStagedOutput.

struct GpuDirectOutputConfig
{
  QRhi* rhi{};
  const score::gfx::RenderState* state{};
  QRhiTexture* sourceTexture{};
  int width{};
  int height{};
  std::uint32_t frameByteSize{};
  int slotCount{2};
  /// Debug name for the ring's QRhiBuffers — appears in RenderDoc /
  /// PIX captures, AJA diagnostics, etc.
  const char* debugName{"score-gpu-direct-output"};

  /// Factory invoked once per slot. Each slot owns its own encoder so
  /// the compute pipeline + SRB are bound to the slot's storage buffer.
  std::function<std::unique_ptr<score::gfx::ComputeEncoder>()> encoderFactory;

  /// Color-conversion GLSL snippet passed into `ComputeEncoder::init`.
  /// Typical caller: `score::gfx::colorMatrixOut(...)`.
  QString colorConversion;

  /// Vendor-supplied pin / unpin callbacks. Must both be non-null.
  VendorDmaRegistrar registrar;
};

/**
 * @brief Generic GPU-direct output. Vendor-neutral.
 *
 * Lives in score-plugin-gfx so any addon (AJA, planned Magewell /
 * Rivermax / etc) can consume it without re-implementing the buffer
 * ring, CUDA context, fence and encoder plumbing.
 */
class SCORE_PLUGIN_GFX_EXPORT GpuDirectOutput
{
public:
  GpuDirectOutput() = default;
  ~GpuDirectOutput();

  GpuDirectOutput(const GpuDirectOutput&) = delete;
  GpuDirectOutput& operator=(const GpuDirectOutput&) = delete;

  /**
   * @brief Allocate CUDA context + ring + fence + dispatcher; invoke
   *        the vendor's `registerSlot` once per slot.
   *
   * Returns false on any failure (no CUDA, ring create failed,
   * encoder init failed, vendor register refused). Partial state is
   * released before returning.
   */
  bool init(const GpuDirectOutputConfig& cfg);

  void release();

  bool valid() const noexcept { return m_ring.valid(); }
  std::size_t slotCount() const noexcept { return m_ring.slotCount(); }

  /// Inside an offscreen frame, after the input pipeline rendered into
  /// the source texture. Dispatches the slot's compute encoder + signals
  /// the fence at the next monotonic value.
  void encodeFrame(QRhiCommandBuffer& cb);

  /// After `endOffscreenFrame`: schedule the CUDA-side fence wait (no-op
  /// on D3D11/GL backends) and return the GPU pointer of the just-encoded
  /// slot — the value the vendor's submission API consumes. Returns
  /// nullptr if the fence wait fails.
  ///
  /// After the vendor accepts the pointer, call `advance()` to rotate.
  void* prepareNextFrame();

  /// Rotate the ring. Call after `prepareNextFrame()` and after the
  /// vendor has accepted the pointer.
  void advance();

private:
  GpuDirectOutputConfig m_cfg{};
  CudaP2PContextHandle m_cudaCtx{};
  GpuRingBuffer m_ring;
  std::unique_ptr<InteropFence> m_fence;
  ComputeRingDispatcher m_dispatcher;
  std::vector<bool> m_pinned;  ///< parallel to slots, tracks vendor register success
  std::uint64_t m_fenceValue{0};
};

} // namespace score::gfx::interop

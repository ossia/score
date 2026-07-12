#pragma once

/**
 * @file InteropFence.hpp
 * @brief Backend-uniform fence between QRhi command-buffer + CUDA stream.
 *
 * Vendor-direct video paths (AJA, Magewell, Rivermax, ...) need to be sure
 * the GPU has finished writing a buffer before the peer device DMA-reads
 * it. The synchronisation primitive differs by graphics API:
 *
 *   - D3D11   : no-op. The immediate context flush done at
 *               `endOffscreenFrame` is sufficient; CUDA mapped resources
 *               share the same queue.
 *   - OpenGL  : `glFinish()`. CPU-stalls but cheap when called once per
 *               frame at 25-60 Hz, and required because GL+CUDA buffer
 *               interop has no native fence path on every driver.
 *   - D3D12   : ID3D12Fence shared with CUDA via `cuda_interop_import_d3d12_fence`
 *               + `cuda_interop_wait_semaphore` — stub today.
 *   - Vulkan  : `VK_KHR_timeline_semaphore` (exported via
 *               OPAQUE_FD / OPAQUE_WIN32) imported into CUDA via
 *               `cuda_interop_import_vulkan_semaphore` + `cuda_interop_wait_semaphore`
 *               — stub today.
 *
 * Per-frame lifecycle on D3D12/Vulkan (once those are real):
 *
 *   // inside offscreen frame, after compute dispatch:
 *   fence.signalAfterEncode(cb, ++value);
 *   // outside the frame, before handing the buffer to the peer:
 *   fence.waitOnCuda(value);
 *
 * Replaces the ad-hoc `glFinish` + comment-on-D3D11-immediate-context-flush
 * pattern duplicated across `RdmaInteropD3D11Tier3` and `RdmaInteropGLTier3`,
 * and gives a stable hook for the Vulkan/D3D12 paths when they land.
 */

#include <Gfx/Graph/interop/CudaInterop.h>
#include <score_plugin_gfx_export.h>

#include <cstdint>
#include <memory>

class QRhi;
class QRhiCommandBuffer;

namespace score::gfx::interop
{

/**
 * @brief Backend-uniform fence interface. Per-vendor adapters hold one
 *        instance, signal it after the per-frame compute dispatch, wait
 *        on it from CUDA before peer DMA.
 */
struct SCORE_PLUGIN_GFX_EXPORT InteropFence
{
  virtual ~InteropFence() = default;

  /// True if init() succeeded. Stub backends return false here so vendor
  /// adapters can detect them and route around.
  virtual bool valid() const noexcept = 0;

  /// One-time setup. Allocates the underlying fence/semaphore and
  /// imports it into the provided CUDA context. Returns false on any
  /// failure (no init partial state).
  virtual bool init(QRhi& rhi, CudaInteropContextHandle cudaCtx) = 0;

  /// One-time teardown. Releases the CUDA semaphore + the underlying
  /// fence/semaphore. Safe to call multiple times.
  virtual void release() = 0;

  /// Inside the offscreen frame, after the compute dispatch: signal the
  /// fence with `value`. Caller increments `value` monotonically.
  /// On D3D11/GL this is a no-op (the implicit per-frame fence suffices).
  virtual void signalAfterEncode(QRhiCommandBuffer& cb, std::uint64_t value) = 0;

  /// After `endOffscreenFrame`, before handing the buffer to the peer:
  /// schedule a CUDA-side wait on `value`. Returns false on bridge error.
  /// On D3D11 this is a no-op (immediate-context flush already ran).
  /// On GL this issues `glFinish` once per frame.
  virtual bool waitOnCuda(std::uint64_t value) = 0;
};

/**
 * @brief Factory. Returns the right concrete impl for `rhi`'s backend.
 *        Caller still needs to `init()` the returned fence. Never null —
 *        unsupported backends return a stub whose `valid()` is false.
 */
SCORE_PLUGIN_GFX_EXPORT
std::unique_ptr<InteropFence> makeInteropFence(QRhi& rhi);

} // namespace score::gfx::interop

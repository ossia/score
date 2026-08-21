#pragma once

/**
 * @file RdmaPlayoutProbe.hpp
 * @brief Content-verified one-time probe of the GPU->peer (playout) P2P path.
 *
 * Pinning a CUDA buffer for a capture card only proves nvidia_p2p_get_pages
 * accepted the range. It does not prove the card can actually *read* that
 * memory over PCIe: with the IOMMU in full-translation mode, or with the card
 * and the GPU behind different host bridges, the transfer is silently dropped
 * while every SDK call still returns success. The card then plays out whatever
 * its framestore already held — a constant frame — and the pump reports a full
 * frame count with zero drops.
 *
 * The capture direction is already content-verified by its vendor adapter
 * (seed the pinned buffer, run a real card->GPU transfer, check the bytes
 * changed). This is the playout counterpart, and it lives here rather than in
 * an addon so every vendor gets the same assertion: seed the pinned buffer with
 * a known non-degenerate pattern, ask the vendor to push it to the device, ask
 * the vendor to read it back, and compare 1:1.
 *
 * The GPU seeding is injected (`RdmaPlayoutProbeIo`) so the decision table is
 * exercisable without a CUDA driver.
 */

#include <Gfx/Graph/interop/VendorDmaRegistrar.hpp>
#include <score_plugin_gfx_export.h>

#include <cstdint>
#include <functional>

namespace score::gfx::interop
{

enum class RdmaPlayoutProbeResult
{
  /// Pushed and read back byte-identical: the peer really reads GPU memory.
  Delivered,
  /// Pushed successfully, but the vendor supplied no readback hook, so
  /// delivery rests on a return code alone.
  Unverified,
  /// The vendor supplied no `verifyTransfer` at all — nothing was probed.
  NoProbe,
  /// The pattern could not be written into the pinned GPU buffer.
  SeedFailed,
  /// `verifyTransfer` returned false.
  TransferFailed,
  /// `readbackTransfer` returned false.
  ReadbackFailed,
  /// The peer returned bytes that are not the ones it was given.
  ContentMismatch,
};

/// Host->GPU access the probe needs. Production callers wrap
/// `cuda_interop_upload_buffer`.
struct RdmaPlayoutProbeIo
{
  std::function<bool(void* gpuDst, const void* hostSrc, std::uint32_t bytes)>
      seedGpu;
};

/// Bytes the probe seeds and compares: enough to cross several GPU pages,
/// capped so the probe stays cheap at 8K rasters.
inline constexpr std::uint32_t rdmaPlayoutProbeMaxBytes = 64u * 1024u;

/// Deterministic, non-degenerate pattern. Never constant and never all-zero,
/// so a dropped transfer (stale framestore, zero-filled scratch) cannot alias
/// a correct result.
SCORE_PLUGIN_GFX_EXPORT
unsigned char rdmaPlayoutProbeByte(std::uint32_t index) noexcept;

/// Run the probe. @p pinnedGpuPtr must already be pinned by the vendor.
SCORE_PLUGIN_GFX_EXPORT
RdmaPlayoutProbeResult rdmaProbePlayoutPath(
    void* pinnedGpuPtr, std::uint32_t frameByteSize,
    const VendorDmaRegistrar& registrar, const RdmaPlayoutProbeIo& io) noexcept;

/// True for the results on which an RDMA output rung may engage.
SCORE_PLUGIN_GFX_EXPORT
bool rdmaPlayoutProbeEngages(RdmaPlayoutProbeResult r) noexcept;

SCORE_PLUGIN_GFX_EXPORT
const char* rdmaPlayoutProbeMessage(RdmaPlayoutProbeResult r) noexcept;

} // namespace score::gfx::interop

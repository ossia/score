#pragma once

/**
 * @file RdmaRingDepth.hpp
 * @brief Shared BAR1-aware slot-count policy for the RDMA rings.
 *
 * Every pinned RDMA bounce/slot occupies the GPU's PCIe BAR1 aperture
 * (256 MiB on most Quadros) for the card's P2P access. At large rasters
 * (UHD2/8K ~ 32-66 MB frames) a full-depth ring on both the capture and the
 * output side would starve the aperture, so the depth is reduced above a byte
 * threshold. Shared by the GL and Vulkan capture and output RDMA shims so
 * all four agree on the same policy.
 */

#include <cstdint>

namespace score::gfx::interop
{

/// The two ring depths a caller picks between: @p full below the BAR1 pressure
/// threshold, @p large at/above it (fewer, larger pinned slots).
struct RdmaRingDepths
{
  int full{};
  int large{};
};

/// Frames >= 32 MiB use the reduced (`large`) depth so the pinned slots still
/// fit the BAR1 aperture; smaller frames use the `full` depth.
inline int rdmaRingDepthForFrame(std::uint32_t frameByteSize, RdmaRingDepths d) noexcept
{
  return frameByteSize >= (32u << 20) ? d.large : d.full;
}

} // namespace score::gfx::interop

#pragma once

/**
 * @file DrmPixelFormat.hpp
 * @brief DRM fourcc <-> VideoPixelFormat, and the PipeWire SPA formats.
 *
 * A DRM fourcc names the component order of a little-endian machine word, so
 * DRM_ARGB8888 is B,G,R,A in memory -- the reverse of how the name reads. That
 * inversion is the single most common source of red/blue swaps on the dma-buf
 * path, so the mapping states the memory order in each comment.
 *
 * A DRM *format modifier* (tiling, compression) is deliberately absent. It is
 * not part of a pixel format: the same fourcc can arrive linear or tiled, and
 * folding one into the other would be the same category error as baking a stride
 * alignment into a format descriptor. Callers pass modifiers alongside.
 *
 * PipeWire's SPA video formats live here too, since they are defined in DRM
 * terms and reached the rest of score through DRM already.
 *
 * Deliberately header-free of libdrm and libspa: the fourccs are computed from
 * their characters, so this builds and is tested everywhere.
 */

#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <score_plugin_gfx_export.h>

#include <cstdint>

namespace score::gfx::interop
{

/// Build a DRM fourcc from its characters, least-significant byte first.
constexpr uint32_t drmPixelFourcc(char a, char b, char c, char d) noexcept
{
  return uint32_t(uint8_t(a)) | (uint32_t(uint8_t(b)) << 8)
         | (uint32_t(uint8_t(c)) << 16) | (uint32_t(uint8_t(d)) << 24);
}

/// The layout behind a DRM fourcc, or Unknown when score has no row for it.
SCORE_PLUGIN_GFX_EXPORT
VideoPixelFormat fromDrmFourcc(uint32_t fourcc) noexcept;

/// The DRM fourcc a layout should be exported as, or 0 when DRM has none.
/// Not a strict inverse: several fourccs can share a layout, and this returns
/// the canonical one.
SCORE_PLUGIN_GFX_EXPORT
uint32_t toDrmFourcc(VideoPixelFormat f) noexcept;

} // namespace score::gfx::interop

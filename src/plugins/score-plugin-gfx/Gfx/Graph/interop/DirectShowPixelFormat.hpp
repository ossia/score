#pragma once

/**
 * @file DirectShowPixelFormat.hpp
 * @brief DirectShow / Video-for-Windows fourcc <-> VideoPixelFormat.
 *
 * The YUV `MEDIASUBTYPE_*` GUIDs are all of the form
 * `{fourcc, 0x0000, 0x0010, {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}}`, so the
 * subtype reduces to the fourcc in `Data1`. Keeping the table in those terms
 * makes it portable and unit-testable on any host, instead of hiding a mapping
 * nobody can exercise inside Windows-only code. The RGB subtypes are genuine
 * SDK GUIDs rather than fourccs and stay with the DirectShow enumeration.
 *
 * Deliberately not platform-guarded: the point is that it builds and is tested
 * everywhere.
 */

#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <score_plugin_gfx_export.h>

#include <cstdint>

namespace score::gfx::interop
{

/// Build a fourcc the way DirectShow spells it, least-significant byte first.
constexpr uint32_t directShowFourcc(char a, char b, char c, char d) noexcept
{
  return uint32_t(uint8_t(a)) | (uint32_t(uint8_t(b)) << 8)
         | (uint32_t(uint8_t(c)) << 16) | (uint32_t(uint8_t(d)) << 24);
}

/// The layout behind a DirectShow YUV fourcc, or Unknown for compressed,
/// RGB-GUID and unhandled subtypes.
SCORE_PLUGIN_GFX_EXPORT
VideoPixelFormat fromDirectShowFourcc(uint32_t fourcc) noexcept;

/// True when the fourcc names a compressed stream rather than a raw layout, so
/// the caller reaches for a decoder instead of a pixel format.
SCORE_PLUGIN_GFX_EXPORT
bool isDirectShowCompressedFourcc(uint32_t fourcc) noexcept;

} // namespace score::gfx::interop

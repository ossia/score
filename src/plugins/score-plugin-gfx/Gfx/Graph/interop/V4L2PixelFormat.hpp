#pragma once

/**
 * @file V4L2PixelFormat.hpp
 * @brief The one V4L2 fourcc <-> VideoPixelFormat table.
 *
 * Raw buffer layouts only. Compressed fourccs (MJPEG, JPEG, H.264, MPEG-4,
 * CPIA) belong to the codec axis, not to a pixel-format vocabulary, and are
 * resolved where the camera is enumerated.
 *
 * There used to be two independent V4L2 tables -- one to AVPixelFormat for
 * camera enumeration, one to VideoPixelFormat for the DMA capture path -- and
 * they disagreed. Anything needing an AVPixelFormat now goes through
 * VideoPixelFormatAV rather than maintaining a second table.
 */

#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <score_plugin_gfx_export.h>

#include <cstdint>

namespace score::gfx::interop
{

/// The layout behind a V4L2 fourcc, or Unknown for compressed and unhandled
/// fourccs. Takes the fourcc as a plain uint32_t so callers need no V4L2 header.
SCORE_PLUGIN_GFX_EXPORT
VideoPixelFormat fromV4L2PixelFormat(uint32_t fourcc) noexcept;

/// The V4L2 fourcc a layout should be requested as, or 0 when V4L2 has none.
/// Not a strict inverse: several fourccs alias onto one layout (the deprecated
/// RGB32/BGR32 pair, Y16/Z16), and this returns the canonical one.
SCORE_PLUGIN_GFX_EXPORT
uint32_t toV4L2PixelFormat(VideoPixelFormat f) noexcept;

} // namespace score::gfx::interop

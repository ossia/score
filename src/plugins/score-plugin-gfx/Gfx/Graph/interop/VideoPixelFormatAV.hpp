#pragma once

/**
 * @file VideoPixelFormatAV.hpp
 * @brief Bridge between the wire-format enum `VideoPixelFormat` and libav's
 *        `AVPixelFormat`.
 *
 * Why two enums exist (and why we can't just use AVPixelFormat everywhere):
 * the SDI/HDMI capture cards carry several *packed wire formats* that FFmpeg
 * does NOT model as pixel formats — it models them as **codecs** that decode
 * into a planar AVPixelFormat:
 *
 *   v210      -> AV_CODEC_ID_V210  (decodes to yuv422p10le) — no AV_PIX_FMT_V210
 *   v216      -> AV_CODEC_ID_V210X (yuv422p16)              — no AV_PIX_FMT
 *   r210/RGB10-> AV_CODEC_ID_R210/R10K (gbrp10/rgb48)       — no exact AV_PIX_FMT
 *   DPX 10/12 -> AV_CODEC_ID_DPX                            — no AV_PIX_FMT
 *   12-bit packed RGB, A2-ARGB10                            — no AV_PIX_FMT
 *
 * Those are exactly the formats capture cards put on the wire, so a wire-format
 * enum is unavoidable. But the *buffer* formats (planar/semi-planar YUV, packed
 * 8-bit RGB, 16-bit RGB, grey) all have AVPixelFormat twins; this bridge maps
 * them so:
 *   - the capture path can derive a `VideoPixelFormat` from the
 *     `Video::ImageFormat::pixel_format` (an AVPixelFormat) it already carries;
 *   - vendor format tables can interop with the libav-based decoder/encoder
 *     paths for the representable subset, with a single source of truth for the
 *     overlap instead of each site re-deriving it.
 *
 * `toAVPixelFormat` returns `AV_PIX_FMT_NONE` for the wire-only formats (the
 * caller keeps using `VideoPixelFormat` for those). `fromAVPixelFormat` returns
 * `VideoPixelFormat::Unknown` for AVPixelFormats with no wire-format equivalent.
 */

#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <score_plugin_gfx_export.h>

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace score::gfx::interop
{

/// Map a wire format to its byte-identical AVPixelFormat, or AV_PIX_FMT_NONE if
/// FFmpeg has no pixel-format for it (v210/v216/DPX/r210/12-bit-packed/A2-ARGB10
/// — those are codecs in FFmpeg, not pixel formats).
SCORE_PLUGIN_GFX_EXPORT
AVPixelFormat toAVPixelFormat(VideoPixelFormat f) noexcept;

/// Map an AVPixelFormat back to a wire format, or VideoPixelFormat::Unknown if
/// it isn't one of the capture-card wire formats.
SCORE_PLUGIN_GFX_EXPORT
VideoPixelFormat fromAVPixelFormat(AVPixelFormat f) noexcept;

} // namespace score::gfx::interop

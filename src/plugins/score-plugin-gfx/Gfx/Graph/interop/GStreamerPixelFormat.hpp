#pragma once

/**
 * @file GStreamerPixelFormat.hpp
 * @brief GStreamer video-format name <-> VideoPixelFormat.
 *
 * GStreamer names formats with strings in caps ("NV12", "I420", "v210"), and
 * `Video::gstreamerToLibav()` already maps those to AVPixelFormat for the decode
 * path. That mapping stays: most of what arrives over GStreamer is a *stream*
 * format, and the decode pipeline is AV-based.
 *
 * This table exists for the transports that hand over a *buffer* -- Sh4lt and
 * Shmdata pass shared memory -- where the layout is what matters, because a
 * layout plus a stride is what lets a frame be imported straight onto the GPU
 * instead of being converted on the CPU first.
 *
 * It is a direct table rather than a composition of the existing map with the
 * libav bridge, and that is the point: going through AVPixelFormat loses the
 * V-before-U layouts, since FFmpeg has no pixel format for them. "YV12" would
 * arrive as YUV420P with red and blue exchanged.
 */

#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <score_plugin_gfx_export.h>

#include <string_view>

namespace score::gfx::interop
{

/// The layout behind a GStreamer format name, or Unknown when score has no row
/// for it -- which includes the formats that are streams rather than buffers.
SCORE_PLUGIN_GFX_EXPORT
VideoPixelFormat fromGStreamerFormat(std::string_view name) noexcept;

/// The GStreamer name for a layout, or an empty view when it has none.
SCORE_PLUGIN_GFX_EXPORT
std::string_view toGStreamerFormat(VideoPixelFormat f) noexcept;

} // namespace score::gfx::interop

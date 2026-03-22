#pragma once

#include <Gfx/Graph/RenderState.hpp>
#include <Video/GpuFormats.hpp>

#include <string>

class QRhi;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
#if __has_include(<libavutil/hwcontext.h>)
#include <libavutil/hwcontext.h>
#endif
}

#include <score_plugin_gfx_export.h>

namespace score::gfx
{

/// Delegates to Video::hwCodecName (score-plugin-gfx wrapper)
SCORE_PLUGIN_GFX_EXPORT
std::string hwCodecName(const char* codec_name, AVHWDeviceType device);

/// Delegates to Video::codecSupportsHWPixelFormat (score-plugin-gfx wrapper)
SCORE_PLUGIN_GFX_EXPORT
bool codecSupportsHWPixelFormat(AVCodecID codec_id, AVPixelFormat pix_fmt);

/// Picks the best HW accel for the given API/codec, using QRhi for vendor detection.
SCORE_PLUGIN_GFX_EXPORT
AVPixelFormat selectHardwareAcceleration(
    score::gfx::GraphicsApi api, AVCodecID codec_id,
    QRhi* rhi = nullptr);

} // namespace score::gfx

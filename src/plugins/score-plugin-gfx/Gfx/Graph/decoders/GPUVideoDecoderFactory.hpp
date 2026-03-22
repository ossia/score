#pragma once
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

#include <Video/VideoInterface.hpp>

#include <memory>
#include <string>

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace score::gfx
{

/**
 * @brief Creates the appropriate GPUVideoDecoder for a given pixel format.
 *
 * This factory centralizes the pixel-format-to-decoder switch statement
 * that was previously duplicated in VideoNodeRenderer and DirectVideoNodeRenderer.
 *
 * @param format The video frame format (width, height, pixel_format)
 * @param filter Optional GLSL filter expression to apply in the fragment shader
 * @return A unique_ptr to the created decoder, or nullptr if format is unhandled
 */
SCORE_PLUGIN_GFX_EXPORT
std::unique_ptr<GPUVideoDecoder> createGPUVideoDecoder(
    Video::ImageFormat& format, const std::string& filter = {});

} // namespace score::gfx

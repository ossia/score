#pragma once

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Video/VideoInterface.hpp>

extern "C" {
#include <libavutil/pixdesc.h>
}

#define SCORE_GFX_VIDEO_UNIFORMS \
"layout(std140, binding = 0) uniform renderer_t {\n" \
"  mat4 clipSpaceCorrMatrix;\n" \
"  vec2 renderSize;\n" \
"} renderer;\n" \
"\n" \
"layout(std140, binding = 2) uniform material_t {\n" \
"  vec2 scale;\n" \
"  vec2 texSz;\n" \
"} mat;\n"

namespace score::gfx
{

/// Describes the pixel layout properties relevant for GPU decoding.
/// Extracted from AVPixFmtDescriptor / codec parameters.
struct PixelFormatInfo
{
  int log2ChromaW{1}; ///< Horizontal chroma subsampling: 0=4:4:4, 1=4:2:2/4:2:0
  int log2ChromaH{1}; ///< Vertical chroma subsampling: 0=4:4:4/4:2:2, 1=4:2:0
  int bitDepth{8};    ///< Per-component bit depth (8, 10, 12, 16)
  int numPlanes{2};   ///< Number of planes (2 for semi-planar, 3+ for planar)
  bool hasAlpha{};    ///< Format includes an alpha channel

  bool is10bit() const noexcept { return bitDepth > 8; }

  /// Build from an AVPixelFormat.
  static PixelFormatInfo fromAVPixelFormat(AVPixelFormat fmt)
  {
    PixelFormatInfo info;
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(fmt);
    if(desc)
    {
      info.log2ChromaW = desc->log2_chroma_w;
      info.log2ChromaH = desc->log2_chroma_h;
      info.bitDepth = desc->comp[0].depth;
      info.numPlanes = av_pix_fmt_count_planes(fmt);
      info.hasAlpha = (desc->flags & AV_PIX_FMT_FLAG_ALPHA) != 0;
    }
    return info;
  }

  /// Build from codec parameters, with optional sw_format override.
  /// bits_per_raw_sample from the codec is used as a hint when > 8.
  static PixelFormatInfo fromCodecParameters(
      AVPixelFormat swFormat, AVPixelFormat codecparFormat,
      int bitsPerRawSample)
  {
    PixelFormatInfo info;
    // Prefer sw_format (from hw_frames_ctx) when available
    AVPixelFormat fmt = (swFormat != AV_PIX_FMT_NONE) ? swFormat : codecparFormat;
    if(fmt != AV_PIX_FMT_NONE)
      info = fromAVPixelFormat(fmt);
    // bits_per_raw_sample overrides depth when set (some containers report it)
    if(bitsPerRawSample > info.bitDepth)
      info.bitDepth = bitsPerRawSample;
    return info;
  }
};

// TODO the "model" nodes should have a first update step so that they
// can share data across all renderers during a tick
class VideoNode;

/**
 * @brief Processes and renders a video frame on the GPU
 *
 * This class is used as a base type for GPU decoders.
 *
 * Child classes must :
 *
 * - Create relevant shaders, samplers & textures in the init method.
 * - When exec is called, copy the data from the AVFrame to the QRhiTextures.
 *
 * See RGB0Decoder for an example with a single texture, YUV420Decoder for an
 * example with multiple textures.
 */
class GPUVideoDecoder
{
public:
  GPUVideoDecoder();
  virtual ~GPUVideoDecoder();

  /**
   * @brief Initialize a GPUVideoDecoder
   *
   * This method must :
   * - Create samplers and textures for the video format.
   * - Create shaders that will render the data put into these textures.
   *
   * It returns a {vertex, fragment} shader pair.
   */
  [[nodiscard]] virtual std::pair<QShader, QShader> init(RenderList& r) = 0;

  /**
   * @brief Decode and upload a video frame to the GPU.
   */
  virtual void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) = 0;

  /**
   * @brief This method will release all the created samplers and textures.
   */
  void release(RenderList&);

  /**
   * @brief Utility method to create a QRhiTextureSubresourceUploadDescription.
   *
   * If possible, it tries to avoid a copy of pixels : pixels must not be freed before the
   * frame has been rendered.
   */
  static QRhiTextureSubresourceUploadDescription
  createTextureUpload(uint8_t* pixels, int w, int h, int bytesPerPixel, int stride);

  static QString vertexShader(bool invertY = false) noexcept;

  std::vector<Sampler> samplers;

  /// Set by exec() on first successful frame upload.
  /// Renderers should skip the render pass until this is true to avoid
  /// showing uninitialized textures (green frame from zero YUV).
  bool hasFrame{};

  /// Set by exec() when the decoder detects an unrecoverable incompatibility
  /// with the frame data (e.g. wrong plane count, unsupported CVPixelBuffer format).
  /// The renderer should check this and rebuild with a fallback decoder.
  bool failed{};
};

/**
 * @brief Default decoder when we do not know what to render.
 */
struct EmptyDecoder : GPUVideoDecoder
{
  static const constexpr auto hashtag_no_filter = R"_(#version 450
    void main ()
    {
    }
  )_";

  explicit EmptyDecoder() { }

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    return score::gfx::makeShaders(r.state, vertexShader(), hashtag_no_filter);
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override { }
};
}

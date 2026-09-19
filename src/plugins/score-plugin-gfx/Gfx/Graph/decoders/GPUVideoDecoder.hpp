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
"  vec4 field;\n" \
"} mat;\n" \
SCORE_GFX_VIDEO_SAMPLE_TRANSFORM

/**
 * The seam every decoder samples through.
 *
 * Colour conversion is generic because every decoder calls convert_to_rgb() at
 * one point and the matrix is injected beside it. Sampling gets the same
 * treatment here: all 85 sampling calls across the decoders read
 * texture(<plane>, score_tc(v_texcoord)), so a per-sample coordinate transform
 * has one place to live and no decoder has to know about it.
 *
 * Today that transform is deinterlacing. A source that delivers FIELDS rather
 * than frames -- NDI does when asked for 16-bit, and capture cards do for
 * interlaced standards -- uploads each field into half of one full-height
 * texture: field 0 (the even lines) into the top half, field 1 (the odd lines)
 * into the bottom half. Each upload is one contiguous region, so nothing is
 * copied to weave them; the weave happens here, per sample.
 *
 * mat.field = (parity of the newest field, mode, 0, 0), mode being
 *   0 progressive -- identity, and the only branch a normal video ever takes
 *   1 weave       -- each output line reads the field that owns its parity
 *   2 bob         -- only the newest field, interpolated to full height
 *
 * The bob case subtracts the parity from the frame line before halving it,
 * which is the half-line offset: field 1's lines sit half a line below field
 * 0's, and without it the picture jitters vertically at the field rate. That
 * is the classic bug in naive bob implementations and it is one term here.
 */
#define SCORE_GFX_VIDEO_SAMPLE_TRANSFORM \
"vec2 score_tc(vec2 tc) {\n" \
"  float mode = mat.field.y;\n" \
"  if(mode < 0.5) return tc;\n" \
"\n" \
"  float lines = max(mat.texSz.y, 2.0);\n" \
"  float halfLines = lines * 0.5;\n" \
"  float parity = mat.field.x;\n" \
"\n" \
"  if(mode < 1.5) {\n" \
"    float y = floor(tc.y * lines);\n" \
"    float lineParity = mod(y, 2.0);\n" \
"    float row = floor(y * 0.5) + 0.5;\n" \
"    return vec2(tc.x, (row / halfLines) * 0.5 + lineParity * 0.5);\n" \
"  }\n" \
"\n" \
"  float k = (tc.y * lines - parity) * 0.5;\n" \
"  k = clamp(k, 0.0, halfLines);\n" \
"  return vec2(tc.x, (k / halfLines) * 0.5 + parity * 0.5);\n" \
"}\n"

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
class SCORE_PLUGIN_GFX_EXPORT GPUVideoDecoder
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
  virtual void release(RenderList&);

  /**
   * @brief Utility method to create a QRhiTextureSubresourceUploadDescription.
   *
   * If possible, it tries to avoid a copy of pixels : pixels must not be freed before the
   * frame has been rendered.
   */
  static QRhiTextureSubresourceUploadDescription
  createTextureUpload(uint8_t* pixels, int w, int h, int bytesPerPixel, int stride);

  /**
   * @brief Whether this frame carries the EVEN lines of its picture.
   *
   * NDI calls that field 0 and marks it top-field-first; the odd lines are
   * field 1. Only meaningful when the format says Interlacing::Fields.
   */
  static bool isTopField(const AVFrame& frame) noexcept
  {
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 0, 0)
    return (frame.flags & AV_FRAME_FLAG_TOP_FIELD_FIRST) != 0;
#else
    return frame.top_field_first != 0;
#endif
  }

  /**
   * @brief The upload geometry for one plane of a possibly-fielded frame.
   *
   * A fielded source hands over half-height frames for a full-height picture.
   * Each field is uploaded into its own half of the texture -- field 0 on top,
   * field 1 below -- so the two halves together are one stacked texture that
   * score_tc samples by parity. Each upload stays one contiguous region, which
   * is what keeps the path free of any CPU weave.
   *
   * @param textureRows the plane's rows in the TEXTURE, i.e. for the picture.
   * @return {rows this frame carries, row offset to upload them at}.
   */
  struct PlaneRows
  {
    int rows{};
    int offset{};
  };
  static PlaneRows planeRows(
      const Video::ImageFormat& fmt, const AVFrame& frame, int textureRows) noexcept
  {
    if(fmt.interlacing != Video::Interlacing::Fields)
      return {textureRows, 0};

    const int half = textureRows / 2;
    return {half, isTopField(frame) ? 0 : half};
  }

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

  /// Set by exec() when the decoder detects a mid-stream change of the actual
  /// pixel format (e.g. HWTransferDecoder's software transfer format changing).
  /// The decoder MUST NOT free/rebuild its own textures & samplers in that case:
  /// the owning renderer's pipeline SRBs are still baked with the current
  /// pointers and may be sampled this frame. Instead it records the new format
  /// and raises this flag so the renderer rebuilds the decoder and its pipelines
  /// together (via setupGpuDecoder()) — recreating textures and SRBs in lockstep.
  bool formatChanged{};
};

/**
 * @brief Default decoder when we do not know what to render.
 */
struct EmptyDecoder : GPUVideoDecoder
{
  // Writing nothing leaves the attachment undefined: NVIDIA hands back black,
  // llvmpipe hands back white. Write the black the callers expect.
  static const constexpr auto hashtag_no_filter = R"_(#version 450
    layout(location = 0) out vec4 fragColor;
    void main ()
    {
      fragColor = vec4(0.0, 0.0, 0.0, 1.0);
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

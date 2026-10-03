#pragma once
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{

/**
 * @brief The shape of a planar YUV format: chroma subsampling, sample size,
 * and an optional alpha plane.
 */
struct PlanarYUVLayout
{
  int log2ChromaW{1}; ///< 1 halves the chroma planes' width, 2 quarters it.
  int log2ChromaH{1};
  int bytesPerSample{1};
  /// Brings a sample to the 8-bit-referenced colour matrices.
  const char* scale{"1.0"};
  bool alpha{false};
};

/**
 * @brief Decodes planar YUV of any subsampling, at 8 or 16 bits, with or
 * without alpha.
 *
 * The formats with their own decoder (YUV420, YUV422, YUV444 and their 10 /
 * 12-bit variants) keep it; this one covers the rest found in real files:
 * 4:1:0 (Indeo, SVQ1), 4:1:1 (DV NTSC), 4:2:2 with alpha (Canopus HQA), and
 * 16-bit 4:2:0 / 4:2:2 (Pixlet, HQX, 012v).
 */
struct PlanarYUVDecoder : GPUVideoDecoder
{
  // %1 = user filter, %2 = colour matrix, %3 = sample scale,
  // %4 = chroma sample coordinate, %5 = alpha
  static const constexpr auto frag = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D y_tex;
layout(binding=4) uniform sampler2D u_tex;
layout(binding=5) uniform sampler2D v_tex;
layout(binding=6) uniform sampler2D a_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

%2

vec4 processTexture(vec4 tex) {
  vec4 processed = convert_to_rgb(tex);
  { %1 }
  return processed;
}

void main ()
{
  const float s = %3;
  float y = s * texture(y_tex, score_tc(v_texcoord)).r;
  float u = s * texture(u_tex, %4).r;
  float v = s * texture(v_tex, %4).r;

  vec4 rgb = processTexture(vec4(y, u, v, 1.));
  fragColor = vec4(rgb.rgb, %5);
})_";

  PlanarYUVDecoder(Video::ImageFormat& d, PlanarYUVLayout l, QString f = "")
      : decoder{d}
      , layout{l}
      , filter{std::move(f)}
  {
  }

  Video::ImageFormat& decoder;
  PlanarYUVLayout layout;
  QString filter;

  QSize planeSize(int plane) const noexcept
  {
    if(plane == 1 || plane == 2)
      return {
          AV_CEIL_RSHIFT(decoder.width, layout.log2ChromaW),
          AV_CEIL_RSHIFT(decoder.height, layout.log2ChromaH)};
    return {decoder.width, decoder.height};
  }

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto fmt = layout.bytesPerSample == 2 ? QRhiTexture::R16 : QRhiTexture::R8;

    // The alpha binding is declared whether or not the format has a plane
    // for it: a 1x1 texture stands in, never sampled.
    for(int plane = 0; plane < 4; plane++)
    {
      const auto sz = plane == 3 && !layout.alpha ? QSize{1, 1} : planeSize(plane);
      auto tex = rhi.newTexture(fmt, sz, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    // Chroma at full vertical resolution follows the luma through
    // deinterlacing; vertically subsampled chroma is sampled as YUV420Decoder
    // samples it.
    const QString chromaCoord
        = layout.log2ChromaH == 0 ? "score_tc(v_texcoord)" : "v_texcoord";
    // Alpha is full range at 8 and 16 bits alike: both fill their lane.
    const QString alpha
        = layout.alpha ? "texture(a_tex, score_tc(v_texcoord)).r" : "1.0";

    return score::gfx::makeShaders(
        r.state, vertexShader(),
        QString(frag)
            .arg(filter)
            .arg(colorMatrix(decoder))
            .arg(layout.scale)
            .arg(chromaCoord)
            .arg(alpha));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    const int planes = layout.alpha ? 4 : 3;
    for(int plane = 0; plane < planes; plane++)
    {
      const auto sz = planeSize(plane);
      QRhiTextureUploadEntry entry{
          0, 0,
          createTextureUpload(
              frame.data[plane], sz.width(), sz.height(), layout.bytesPerSample,
              frame.linesize[plane])};
      QRhiTextureUploadDescription desc{entry};
      res.uploadTexture(samplers[plane].texture, desc);
    }
  }
};

}

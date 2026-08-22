#pragma once
#include <Gfx/Graph/decoders/CaptureAdjustGLSL.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

#include <QString>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{
/**
 * @brief Demosaics a single-channel colour-filter-array frame into RGB.
 *
 * The mosaic arrives as one sample per pixel in an R8 or R16 texture and is
 * expanded by bilinear interpolation: at a site carrying its own colour the
 * sample is kept, and the two missing channels are averaged from the four
 * orthogonal and four diagonal neighbours (or, on a green site, from the two
 * horizontal and two vertical ones).
 *
 * The CFA order is a constructor parameter rather than a baked assumption:
 * a capture that crops to an odd origin flips the phase, and a wrong phase
 * does not look broken -- it looks like a colour cast, which is easy to
 * mistake for white balance.
 *
 * `sampleScale` accounts for a mosaic that does not fill its container. Ten
 * bits right-aligned in a 16-bit lane -- what V4L2 specifies for SRGGB10 and
 * friends -- normalise to 1/64 of full scale and need 65535/1023, exactly as
 * Mono10 does. A producer that instead replicates the high bits down into the
 * low ones already spans the full range and needs 1.0.
 *
 * Black level, white balance, exposure and the transfer curve are applied
 * after the reconstruction, from the material block rather than baked in --
 * see CaptureAdjustGLSL.hpp. They are per-sensor corrections, not part of
 * turning a mosaic into RGB, and every one of them is the identity by default.
 * Lens shading is still absent.
 */
struct BayerDecoder : GPUVideoDecoder
{
  /// Offset of the red site within the 2x2 cell, which is what distinguishes
  /// the four CFA orders from one another.
  enum class Phase
  {
    RGGB, ///< red at (0,0)
    GRBG, ///< red at (1,0)
    GBRG, ///< red at (0,1)
    BGGR, ///< red at (1,1)
  };

  // %1 = user filter, %2/%3 = red-site offset, %4 = sample scale
  static const constexpr auto frag = R"_(#version 450

)_" SCORE_GFX_CAPTURE_UNIFORMS SCORE_GFX_CAPTURE_ADJUST_FN R"_(

layout(binding=3) uniform sampler2D u_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

vec4 processTexture(vec4 tex) {
  vec4 processed = tex;
  { %1 }
  return processed;
}

float smp(ivec2 q, ivec2 lim) {
  return texelFetch(u_tex, clamp(q, ivec2(0), lim), 0).r;
}

void main() {
  ivec2 sz = textureSize(u_tex, 0);
  ivec2 lim = sz - ivec2(1);
  ivec2 ip = clamp(ivec2(floor(v_texcoord * vec2(sz))), ivec2(0), lim);

  // (0,0) marks the red site; the other three cells follow from its parity.
  ivec2 c = (ip + ivec2(%2, %3)) & 1;

  float ctr = smp(ip, lim);
  float n  = smp(ip + ivec2( 0,-1), lim);
  float s  = smp(ip + ivec2( 0, 1), lim);
  float w  = smp(ip + ivec2(-1, 0), lim);
  float e  = smp(ip + ivec2( 1, 0), lim);
  float nw = smp(ip + ivec2(-1,-1), lim);
  float ne = smp(ip + ivec2( 1,-1), lim);
  float sw = smp(ip + ivec2(-1, 1), lim);
  float se = smp(ip + ivec2( 1, 1), lim);

  float cross4 = (n + s + w + e) * 0.25;
  float diag4  = (nw + ne + sw + se) * 0.25;
  float horz2  = (w + e) * 0.5;
  float vert2  = (n + s) * 0.5;

  vec3 rgb;
  if(c.x == 0 && c.y == 0)
    rgb = vec3(ctr, cross4, diag4);          // red site
  else if(c.x == 1 && c.y == 1)
    rgb = vec3(diag4, cross4, ctr);          // blue site
  else if(c.x == 1 && c.y == 0)
    rgb = vec3(horz2, ctr, vert2);           // green, red neighbours across
  else
    rgb = vec3(vert2, ctr, horz2);           // green, blue neighbours across

  fragColor = processTexture(vec4(adjustCapture(rgb * %4), 1.0));
}
)_";

  BayerDecoder(
      QRhiTexture::Format fmt, int bytesPerSample, Video::ImageFormat& d,
      Phase phase, double sampleScale = 1.0)
      : format{fmt}
      , bytesPerSample{bytesPerSample}
      , decoder{d}
      , phase{phase}
      , sampleScale{sampleScale}
  {
  }

  QRhiTexture::Format format;
  int bytesPerSample{};
  Video::ImageFormat& decoder;
  Phase phase{};
  double sampleScale{1.0};

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    {
      const bool supported = rhi.isTextureFormatSupported(format);
      auto tex = rhi.newTexture(format, QSize{w, h}, 1, QRhiTexture::Flag{});
      const bool created = tex->create();
      if(!supported || !created)
        qWarning() << "BayerDecoder: the backend will not give us a" << int(format)
                   << "texture at" << w << "x" << h
                   << "(supported:" << supported << "created:" << created
                   << ") -- the mosaic has nowhere to land";
      else
        qDebug() << "BayerDecoder:" << w << "x" << h << "format" << int(format)
                 << "ok";

      // The mosaic is reconstructed per texel: linear filtering would blend
      // neighbouring colour sites together before the demosaic can separate
      // them, which is the same hazard the byte-reassembling decoders have.
      auto sampler = rhi.newSampler(
          QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();

      samplers.push_back({sampler, tex});
    }

    int px = 0, py = 0;
    switch(phase)
    {
      case Phase::RGGB: px = 0; py = 0; break;
      case Phase::GRBG: px = 1; py = 0; break;
      case Phase::GBRG: px = 0; py = 1; break;
      case Phase::BGGR: px = 1; py = 1; break;
    }

    return score::gfx::makeShaders(
        r.state, score::gfx::captureVertexShader,
        QString(frag)
            .arg("")
            .arg(px)
            .arg(py)
            .arg(QString::number(sampleScale, 'f', 6)));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    auto tex = samplers[0].texture;
    QRhiTextureUploadEntry entry{
        0, 0,
        createTextureUpload(
            frame.data[0], decoder.width, decoder.height, bytesPerSample,
            frame.linesize[0])};
    QRhiTextureUploadDescription desc{entry};
    res.uploadTexture(tex, desc);
  }
};

} // namespace score::gfx

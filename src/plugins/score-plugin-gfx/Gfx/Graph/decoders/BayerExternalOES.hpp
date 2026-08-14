#pragma once

/**
 * @file BayerExternalOES.hpp
 * @brief Demosaic a colour-filter-array frame sampled through GL_TEXTURE_EXTERNAL_OES.
 *
 * Same reconstruction as BayerDecoder, over the one texture target Tegra will
 * accept for an imported dma-buf. Measured on an Orin NX against a V4L2 export
 * of the IMX676: eglCreateImage accepts R16, R8 and ABGR8888, binding any of
 * them to GL_TEXTURE_2D fails, and GL_TEXTURE_EXTERNAL_OES succeeds for all
 * three. Without this decoder the zero-copy rung has no consumer and capture
 * falls back to staging 25 MB per frame out of uncached pages.
 *
 * Two constraints separate this from BayerDecoder, and both are the external
 * sampler's:
 *
 *  - `texelFetch` does not exist for `samplerExternalOES`, so the neighbourhood
 *    is gathered by normalised coordinate. `mat.texSz` supplies the geometry
 *    the integer form would have read from `textureSize`.
 *  - the sample arrives in `.r`. Measured: an R16 external image reads back
 *    (v, 0, 0, 1), so the mosaic value is the red channel and nothing else is
 *    populated.
 *
 * NEAREST filtering is mandatory rather than preferred: LINEAR blends adjacent
 * colour sites before the demosaic can separate them.
 */

#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/Bayer.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{

struct BayerExternalOESDecoder : GPUVideoDecoder
{
  // Baked with sampler2D so glslang accepts it; the GLSL variant is swapped to
  // samplerExternalOES after baking, which is the only place the type is legal.
  // %1 = red-site x offset, %2 = red-site y offset, %3 = sample scale.
  static const constexpr auto oes_filter = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main()
{
  vec2 ts = mat.texSz;
  vec2 inv = 1.0 / ts;
  vec2 tp = v_texcoord * ts;
  vec2 base = floor(tp) + 0.5;
  ivec2 ip = ivec2(floor(tp));

  // (0,0) marks the red site; the other three cells follow from its parity.
  ivec2 c = (ip + ivec2(%1, %2)) & 1;

  float ctr = texture(tex, base * inv).r;
  float n  = texture(tex, (base + vec2( 0.0,-1.0)) * inv).r;
  float s  = texture(tex, (base + vec2( 0.0, 1.0)) * inv).r;
  float w  = texture(tex, (base + vec2(-1.0, 0.0)) * inv).r;
  float e  = texture(tex, (base + vec2( 1.0, 0.0)) * inv).r;
  float nw = texture(tex, (base + vec2(-1.0,-1.0)) * inv).r;
  float ne = texture(tex, (base + vec2( 1.0,-1.0)) * inv).r;
  float sw = texture(tex, (base + vec2(-1.0, 1.0)) * inv).r;
  float se = texture(tex, (base + vec2( 1.0, 1.0)) * inv).r;

  float cross4 = (n + s + w + e) * 0.25;
  float diag4  = (nw + ne + sw + se) * 0.25;
  float horz2  = (w + e) * 0.5;
  float vert2  = (n + s) * 0.5;

  vec3 rgb;
  if(c.x == 0 && c.y == 0)
    rgb = vec3(ctr, cross4, diag4);
  else if(c.x == 1 && c.y == 1)
    rgb = vec3(diag4, cross4, ctr);
  else if(c.x == 1 && c.y == 0)
    rgb = vec3(horz2, ctr, vert2);
  else
    rgb = vec3(vert2, ctr, horz2);

  fragColor = vec4(clamp(rgb * %3, 0.0, 1.0), 1.0);
}
)_";

  BayerExternalOESDecoder(
      Video::ImageFormat& d, BayerDecoder::Phase phase, double sampleScale = 1.0)
      : decoder{d}
      , phase{phase}
      , sampleScale{sampleScale}
  {
  }

  Video::ImageFormat& decoder;
  BayerDecoder::Phase phase{};
  double sampleScale{1.0};

  std::pair<QShader, QShader> init(RenderList& r) override;

  /// The strategy owns the image: it re-targets the EGLImage behind this
  /// texture every frame and swaps the texture into this decoder's sampler.
  /// A CPU AVFrame cannot be fed through an external image at all, so reaching
  /// here on a host-staged path is a wiring error rather than something to
  /// paper over.
  void exec(RenderList&, QRhiResourceUpdateBatch&, AVFrame&) override { }
};

}

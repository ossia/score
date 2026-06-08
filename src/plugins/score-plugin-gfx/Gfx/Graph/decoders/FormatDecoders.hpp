#pragma once

/**
 * @file FormatDecoders.hpp
 * @brief Capture-side fragment shaders that unpack vendor pixel formats
 *        into RGBA8 for downstream score::gfx consumers.
 *
 * Symmetric to the existing `Gfx/Graph/encoders/{UYVY,V210,NV12,I420}.hpp`
 * encoders which produce vendor formats from RGBA8. These decoders run
 * the inverse: they sample a host-uploaded texture whose bytes are in
 * the vendor format and emit RGBA8 / RGBA16F.
 *
 * Currently each vendor strategy inlines its own decoder shader (AJA
 * capture path, decklink capture, magewell). Consolidating here lets
 * all vendor capture strategies pick a decoder by `VideoPixelFormat`
 * tag without re-writing the GLSL.
 *
 * Pattern matches the existing encoders: GLSL 450, Qt's QSHADER_*
 * macros gate Y-flip and texelFetch semantics across backends.
 */

#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <score_plugin_gfx_export.h>

namespace score::gfx::interop
{

/** Fragment shader source for UYVY → RGBA decode.
 *
 *  Input texture: BGRA8 storage; width = source_width / 2 (each texel
 *  holds 2 luma + 1 chroma pair: bytes are U Y0 V Y1). Sampler reads
 *  one texel per output column-pair; integer math picks even/odd luma. */
constexpr const char* uyvyDecoderFrag = R"_(#version 450
  layout(location = 0) in vec2 v_texcoord;
  layout(location = 0) out vec4 fragColor;
  layout(binding = 3) uniform sampler2D src_tex;

  // BT.709 limited-range conversion. Override via specialization if
  // the source is BT.601 / full-range.
  vec3 yuvToRgb(float y, float u, float v) {
    y = (y - 16.0/255.0) * 255.0/219.0;
    u = (u - 128.0/255.0) * 255.0/224.0;
    v = (v - 128.0/255.0) * 255.0/224.0;
    return vec3(
      y + 1.5748 * v,
      y - 0.1873 * u - 0.4681 * v,
      y + 1.8556 * u);
  }

  vec2 flip_y_uv(vec2 tc) {
  #if defined(QSHADER_MSL) || defined(QSHADER_HLSL)
    return tc;
  #else
    return vec2(tc.x, 1.0 - tc.y);
  #endif
  }

  void main() {
    vec2 tc = flip_y_uv(v_texcoord);
    ivec2 dim = textureSize(src_tex, 0); // dim.x = source_width/2
    int outX = int(tc.x * float(dim.x) * 2.0);  // output column in source pixels
    int srcX = outX / 2;
    bool even = (outX & 1) == 0;
    vec4 t = texelFetch(src_tex, ivec2(srcX, int(tc.y * float(textureSize(src_tex,0).y))), 0);
    // BGRA8 byte order: R=byte0=U, G=byte1=Y0, B=byte2=V, A=byte3=Y1
    float y = even ? t.g : t.a;
    float u = t.r;
    float v = t.b;
    fragColor = vec4(yuvToRgb(y, u, v), 1.0);
  }
)_";

/** Fragment shader source for V210 → RGBA decode.
 *
 *  Input texture: RGBA8 storage; one texel = one little-endian 32-bit
 *  word of V210. Six source pixels span 4 texels (16 bytes). The
 *  shader does the bitwise unpack and YUV→RGB per pixel. Output width
 *  must be source_width (full luma resolution). */
constexpr const char* v210DecoderFrag = R"_(#version 450
  layout(location = 0) in vec2 v_texcoord;
  layout(location = 0) out vec4 fragColor;
  layout(binding = 3) uniform usampler2D src_tex; // sampled as uvec4 bytes

  // V210 layout (per 4 LE words = 6 pixels):
  //   word0: 10b Cb01 | 10b Y0   | 10b Cr01 | 2b pad
  //   word1: 10b Y1   | 10b Cb23 | 10b Y2   | 2b pad
  //   word2: 10b Cr23 | 10b Y3   | 10b Cb45 | 2b pad
  //   word3: 10b Y4   | 10b Cr45 | 10b Y5   | 2b pad

  vec3 yuvToRgb(float y, float u, float v) {
    y = (y * 1023.0 - 64.0) / 876.0;
    u = (u * 1023.0 - 512.0) / 896.0;
    v = (v * 1023.0 - 512.0) / 896.0;
    return vec3(
      y + 1.5748 * v,
      y - 0.1873 * u - 0.4681 * v,
      y + 1.8556 * u);
  }

  void main() {
    ivec2 dim = textureSize(src_tex, 0); // dim.x = (width+47)/48 * 32
    int outX = int(v_texcoord.x * float(dim.x) * 1.5); // ratio not exact; vendor strategy passes width explicitly
    int outY = int(v_texcoord.y * float(textureSize(src_tex,0).y));
    int groupIdx = outX / 6;
    int pxInGroup = outX % 6;
    int wordBase = groupIdx * 4;

    // Re-fetch raw words. We use a usampler2D so each texel is uvec4 of bytes.
    // Reconstruct the 32-bit LE word from the 4 bytes.
    uvec4 b0 = texelFetch(src_tex, ivec2(wordBase + 0, outY), 0);
    uvec4 b1 = texelFetch(src_tex, ivec2(wordBase + 1, outY), 0);
    uvec4 b2 = texelFetch(src_tex, ivec2(wordBase + 2, outY), 0);
    uvec4 b3 = texelFetch(src_tex, ivec2(wordBase + 3, outY), 0);
    uint w0 = b0.r | (b0.g << 8) | (b0.b << 16) | (b0.a << 24);
    uint w1 = b1.r | (b1.g << 8) | (b1.b << 16) | (b1.a << 24);
    uint w2 = b2.r | (b2.g << 8) | (b2.b << 16) | (b2.a << 24);
    uint w3 = b3.r | (b3.g << 8) | (b3.b << 16) | (b3.a << 24);

    uint cb01 = w0 & 0x3FFu;
    uint y0   = (w0 >> 10) & 0x3FFu;
    uint cr01 = (w0 >> 20) & 0x3FFu;
    uint y1   = w1 & 0x3FFu;
    uint cb23 = (w1 >> 10) & 0x3FFu;
    uint y2   = (w1 >> 20) & 0x3FFu;
    uint cr23 = w2 & 0x3FFu;
    uint y3   = (w2 >> 10) & 0x3FFu;
    uint cb45 = (w2 >> 20) & 0x3FFu;
    uint y4   = w3 & 0x3FFu;
    uint cr45 = (w3 >> 10) & 0x3FFu;
    uint y5   = (w3 >> 20) & 0x3FFu;

    uint y, cb, cr;
    switch(pxInGroup) {
      case 0: y = y0; cb = cb01; cr = cr01; break;
      case 1: y = y1; cb = cb01; cr = cr01; break;
      case 2: y = y2; cb = cb23; cr = cr23; break;
      case 3: y = y3; cb = cb23; cr = cr23; break;
      case 4: y = y4; cb = cb45; cr = cr45; break;
      default: y = y5; cb = cb45; cr = cr45; break;
    }
    fragColor = vec4(yuvToRgb(float(y)/1023.0, float(cb)/1023.0, float(cr)/1023.0), 1.0);
  }
)_";

/** Fragment shader source for NV12 → RGBA decode.
 *
 *  Two-plane input: sampler2D `src_y` (Y plane, width × height, one
 *  byte per texel as R channel of a R8 texture) and sampler2D `src_uv`
 *  (UV interleaved plane, width/2 × height/2, two bytes per texel as
 *  RG of an RG8 texture). Output RGBA at source resolution. */
constexpr const char* nv12DecoderFrag = R"_(#version 450
  layout(location = 0) in vec2 v_texcoord;
  layout(location = 0) out vec4 fragColor;
  layout(binding = 3) uniform sampler2D src_y;
  layout(binding = 4) uniform sampler2D src_uv;

  vec3 yuvToRgb(float y, float u, float v) {
    y = (y - 16.0/255.0) * 255.0/219.0;
    u = (u - 128.0/255.0) * 255.0/224.0;
    v = (v - 128.0/255.0) * 255.0/224.0;
    return vec3(
      y + 1.5748 * v,
      y - 0.1873 * u - 0.4681 * v,
      y + 1.8556 * u);
  }

  void main() {
    float y = texture(src_y, v_texcoord).r;
    vec2 uv = texture(src_uv, v_texcoord).rg;
    fragColor = vec4(yuvToRgb(y, uv.r, uv.g), 1.0);
  }
)_";

/** Return the decode fragment-shader source for `f`. Returns nullptr
 *  if `f` is a format that doesn't need a decode pass (BGRA8 / RGBA8 /
 *  RGBA16F can sample-through directly without a shader). */
SCORE_PLUGIN_GFX_EXPORT
const char* decoderFragmentForFormat(VideoPixelFormat f) noexcept;

} // namespace score::gfx::interop

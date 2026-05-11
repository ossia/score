#pragma once
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{
/**
 * @brief Decodes 10-bit 4:2:2 YCbCr packed in v210 (SMPTE ST 2110-20 /
 *        SDI native).
 *
 * v210 packs 6 source pixels into 4 little-endian 32-bit words (16 bytes):
 *   word 0: Cb01 (10b) | Y0   (10b) | Cr01 (10b) | 2b pad
 *   word 1: Y1   (10b) | Cb23 (10b) | Y2   (10b) | 2b pad
 *   word 2: Cr23 (10b) | Y3   (10b) | Cb45 (10b) | 2b pad
 *   word 3: Y4   (10b) | Cr45 (10b) | Y5   (10b) | 2b pad
 *
 * Inverse of V210Encoder. The input texture is RGBA8 at ((width/6)*4) ×
 * height — each RGBA8 texel is one v210 ULWord, byte-for-byte. For the
 * AJA capture path, that's exactly what DVP DMAs from sysmem to GPU.
 *
 * Width must be a multiple of 6 (and is in practice always a multiple
 * of 48 for SDI: 1920, 3840, 7680). Non-aligned widths would need row
 * padding, which we don't bother with — SDI doesn't produce them.
 *
 * Output: full-width RGB. The fragment shader extracts the 10-bit
 * Y/Cb/Cr fields, normalizes to [0, 1] (10-bit limited range maps to
 * roughly the same [16/255, 235/255] window as 8-bit limited), then
 * runs the standard score colorMatrix() convert_to_rgb. SDR/BT.709 by
 * default; HDR (BT.2020 PQ/HLG) routes through bt2020shader() the same
 * way as any other 10-bit YUV input.
 */
struct V210Decoder : GPUVideoDecoder
{
  // %1 = user filter, %2 = colorMatrix() (emits convert_to_rgb)
  static const constexpr auto frag = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D u_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

%2

vec4 processTexture(vec4 tex) {
  vec4 processed = convert_to_rgb(tex);
  { %1 }
  return processed;
}

uint readWord(int wx, int y) {
  // Sample the RGBA8 texel that holds one v210 ULWord, repack the four
  // bytes into a little-endian uint32 we can bit-extract from.
  vec4 t = texelFetch(u_tex, ivec2(wx, y), 0);
  uint b0 = uint(t.r * 255.0 + 0.5);
  uint b1 = uint(t.g * 255.0 + 0.5);
  uint b2 = uint(t.b * 255.0 + 0.5);
  uint b3 = uint(t.a * 255.0 + 0.5);
  return b0 | (b1 << 8u) | (b2 << 16u) | (b3 << 24u);
}

void main() {
  // Output coordinates are in source-pixel space. mat.texSz is the
  // logical (full-width) frame size that downstream nodes see.
  int outX = int(floor(v_texcoord.x * mat.texSz.x));
  int outY = int(floor(v_texcoord.y * mat.texSz.y));

  int groupIdx   = outX / 6;
  int pixInGroup = outX - groupIdx * 6;
  int wordBaseX  = groupIdx * 4; // 4 ULWords per 6-pixel group

  uint y10 = 0u, cb10 = 0u, cr10 = 0u;

  if (pixInGroup == 0) {
    uint w0 = readWord(wordBaseX + 0, outY);
    cb10 =  w0         & 0x3FFu;
    y10  = (w0 >> 10u) & 0x3FFu;
    cr10 = (w0 >> 20u) & 0x3FFu;
  } else if (pixInGroup == 1) {
    uint w0 = readWord(wordBaseX + 0, outY);
    uint w1 = readWord(wordBaseX + 1, outY);
    cb10 =  w0         & 0x3FFu;       // shared with pixel 0
    y10  =  w1         & 0x3FFu;
    cr10 = (w0 >> 20u) & 0x3FFu;       // shared with pixel 0
  } else if (pixInGroup == 2) {
    uint w1 = readWord(wordBaseX + 1, outY);
    uint w2 = readWord(wordBaseX + 2, outY);
    cb10 = (w1 >> 10u) & 0x3FFu;
    y10  = (w1 >> 20u) & 0x3FFu;
    cr10 =  w2         & 0x3FFu;
  } else if (pixInGroup == 3) {
    uint w1 = readWord(wordBaseX + 1, outY);
    uint w2 = readWord(wordBaseX + 2, outY);
    cb10 = (w1 >> 10u) & 0x3FFu;       // shared with pixel 2
    y10  = (w2 >> 10u) & 0x3FFu;
    cr10 =  w2         & 0x3FFu;       // shared with pixel 2
  } else if (pixInGroup == 4) {
    uint w2 = readWord(wordBaseX + 2, outY);
    uint w3 = readWord(wordBaseX + 3, outY);
    cb10 = (w2 >> 20u) & 0x3FFu;
    y10  =  w3         & 0x3FFu;
    cr10 = (w3 >> 10u) & 0x3FFu;
  } else { // 5
    uint w2 = readWord(wordBaseX + 2, outY);
    uint w3 = readWord(wordBaseX + 3, outY);
    cb10 = (w2 >> 20u) & 0x3FFu;       // shared with pixel 4
    y10  = (w3 >> 20u) & 0x3FFu;
    cr10 = (w3 >> 10u) & 0x3FFu;       // shared with pixel 4
  }

  // 10-bit limited-range YCbCr (Y in [64,940], Cb/Cr in [64,960]) divided
  // by 1023 yields the same [0,1]-normalized signal the 8-bit limited
  // range matrices in ColorSpace.hpp expect, modulo a tiny scale factor
  // that's well below visible quantization. The matrix handles the
  // [16/255, 235/255] -> [0, 1] expansion internally.
  vec4 yuv = vec4(float(y10), float(cb10), float(cr10), 1023.0) / 1023.0;
  fragColor = processTexture(yuv);
}
)_";

  V210Decoder(Video::ImageFormat& d)
      : decoder{d}
  {
  }

  Video::ImageFormat& decoder;

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    // v210 input texture: 4 RGBA8 texels per 6-pixel group.
    const int texW = (w / 6) * 4;
    {
      auto tex
          = rhi.newTexture(QRhiTexture::RGBA8, {texW, h}, 1, QRhiTexture::Flag{});
      tex->create();

      // Nearest sampling: the shader uses texelFetch but the binding
      // model still requires a sampler. Linear would smear bytes
      // across v210 word boundaries — meaningless.
      auto sampler = rhi.newSampler(
          QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();

      samplers.push_back({sampler, tex});
    }

    return score::gfx::makeShaders(
        r.state, vertexShader(), QString(frag).arg("").arg(colorMatrix(decoder)));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    // CPU-staging path: AVFrame.data[0] is raw v210 with row stride =
    // frame.linesize[0] = ((width+47)/48)*128. The texture row is
    // (width/6)*4 RGBA8 texels = width*16/6 bytes; for width % 48 == 0
    // that exactly matches the AJA stride.
    const int texW = (decoder.width / 6) * 4;
    auto y_tex = samplers[0].texture;
    QRhiTextureUploadEntry entry{
        0, 0,
        createTextureUpload(
            frame.data[0], texW, decoder.height, /*bpp=*/4, frame.linesize[0])};
    QRhiTextureUploadDescription desc{entry};
    res.uploadTexture(y_tex, desc);
  }
};

} // namespace score::gfx

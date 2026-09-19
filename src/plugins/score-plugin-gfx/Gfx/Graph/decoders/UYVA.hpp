#pragma once
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

namespace score::gfx
{

/**
 * @brief NDI UYVA: a UYVY 4:2:2 plane, then a full-resolution alpha plane.
 *
 * No AVPixelFormat describes this. ffmpeg's AV_PIX_FMT_UYVA shares the name
 * and not the layout -- it is packed 4:4:4:4 at 32bpp, UYVAUYVA... -- so the
 * decoder is selected through ImageFormat::native_format instead.
 *
 * Plane 0 is UYVY, uploaded as RGBA8 at half width like UYVY422Decoder does;
 * plane 1 is one byte per pixel at full width. The alpha is what this exists
 * for: mapping UYVA to plain UYVY, which is what the NDI input did before,
 * silently drops it.
 */
struct UYVADecoder : GPUVideoDecoder
{
  static const constexpr auto frag = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D y_tex;
layout(binding=4) uniform sampler2D a_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

%2

vec4 processTexture(vec4 tex) {
  vec4 processed = convert_to_rgb(tex);
  { %1 }
  return processed;
}

void main() {
   // UYVY packs U0 Y0 V0 Y1 into RGBA as x=U0 y=Y0 z=V0 w=Y1, so the texture
   // is half the width of the picture and each texel is two pixels.
   float colIndex = floor(v_texcoord.x * mat.texSz.x);
   float oddCol = mod(colIndex, 2.0);

   vec2 tc = score_tc(v_texcoord);
   vec2 dxInput = 0.5 * vec2(1.0 / mat.texSz.x, 0.0);

   float oddY = texture(y_tex, tc - dxInput).w;
   float evenY = texture(y_tex, tc + dxInput).y;
   float y = mix(evenY, oddY, oddCol);
   vec2 uv = texture(y_tex, tc).xz;

   // The alpha plane is full resolution, so it samples at the picture
   // coordinate rather than the half-width one.
   float a = texture(a_tex, tc).r;

   vec4 rgb = processTexture(vec4(y, uv.x, uv.y, 1.));
   fragColor = vec4(rgb.rgb, a);
}
)_";

  explicit UYVADecoder(Video::ImageFormat& d)
      : decoder{d}
  {
  }

  Video::ImageFormat& decoder;

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    auto mkSampler = [&] {
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      return sampler;
    };

    {  // UYVY, RGBA8 at half width
      auto tex = rhi.newTexture(QRhiTexture::RGBA8, {w / 2, h}, 1, QRhiTexture::Flag{});
      tex->create();
      samplers.push_back({mkSampler(), tex});
    }
    {  // alpha, one byte per pixel
      auto tex = rhi.newTexture(QRhiTexture::R8, {w, h}, 1, QRhiTexture::Flag{});
      tex->create();
      samplers.push_back({mkSampler(), tex});
    }

    return score::gfx::makeShaders(
        r.state, vertexShader(), QString(frag).arg("").arg(colorMatrix(decoder)));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    {
      const auto [rows, offset] = planeRows(decoder, frame, decoder.height);
      auto sub = createTextureUpload(
          frame.data[0], decoder.width / 2, rows, 4, frame.linesize[0]);
      sub.setDestinationTopLeft({0, offset});
      res.uploadTexture(samplers[0].texture, QRhiTextureUploadDescription{{0, 0, sub}});
    }
    {
      const auto [rows, offset] = planeRows(decoder, frame, decoder.height);
      auto sub = createTextureUpload(
          frame.data[1], decoder.width, rows, 1, frame.linesize[1]);
      sub.setDestinationTopLeft({0, offset});
      res.uploadTexture(samplers[1].texture, QRhiTextureUploadDescription{{0, 0, sub}});
    }
  }
};

}

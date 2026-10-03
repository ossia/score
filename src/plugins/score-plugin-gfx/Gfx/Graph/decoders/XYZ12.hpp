#pragma once
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{

/**
 * @brief Decodes XYZ12LE, the DCDM X'Y'Z' of JPEG 2000 DCPs.
 *
 * Packed 4:4:4, three little-endian 16-bit words per pixel, each holding a
 * 12-bit code value in its top bits. The layout is RGB48's, so the words go
 * into an R16 texture three texels wide per pixel and are reassembled with
 * texelFetch.
 *
 * The colour pipeline is fixed by SMPTE 428-1 rather than read from the
 * stream: the code values are X'Y'Z' = (XYZ / 52.37)^(1/2.6), with XYZ in
 * cd/m^2 and the reference white at Y = 48 cd/m^2. Decoding is therefore
 *   gamma 2.6 -> scale to 1.0 = 48 cd/m^2 -> XYZ to RGB (D65 primaries).
 * No chromatic adaptation is applied: the DCDM is absolute colorimetry, so a
 * DCI-white master keeps its slightly green-tinted white, as it does in
 * libdcp and ffmpeg.
 */
struct XYZ12Decoder : GPUVideoDecoder
{
  static const constexpr auto frag = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D y_tex;

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
  vec2 tc = score_tc(v_texcoord);
  int x = int(floor(tc.x * mat.texSz.x) * 3.);
  int y = int(tc.y * mat.texSz.y);
  // 12 bits at the top of the word: full scale is 4095 << 4 = 65520.
  const float s = 65535.0 / 65520.0;
  float X = texelFetch(y_tex, ivec2(x + 0, y), 0).r * s;
  float Y = texelFetch(y_tex, ivec2(x + 1, y), 0).r * s;
  float Z = texelFetch(y_tex, ivec2(x + 2, y), 0).r * s;
  fragColor = processTexture(vec4(X, Y, Z, 1.));
})_";

  // SMPTE 428-1 decoding: linear XYZ with 1.0 = the 48 cd/m^2 reference
  // white. The 52.37 cd/m^2 code-value peak lands at 1.091.
  static constexpr auto dcdm_eotf = R"_(
vec3 dcdmToXYZ(vec3 v) {
  return pow(clamp(v, 0.0, 1.0), vec3(2.6)) * (52.37 / 48.0);
}
)_";

  // XYZ -> linear RGB for D65 primaries. GLSL matrices are column-major.
  static constexpr auto xyz_to_bt709 = R"_(
const mat3 xyzToRgb = mat3(
   3.2404542, -0.9692660,  0.0556434,
  -1.5371385,  1.8760108, -0.2040259,
  -0.4985314,  0.0415560,  1.0572252
);
)_";
  static constexpr auto xyz_to_bt2020 = R"_(
const mat3 xyzToRgb = mat3(
   1.7166512, -0.6666844,  0.0176399,
  -0.3556708,  1.6164812, -0.0427706,
  -0.2533663,  0.0157685,  0.9421031
);
)_";

  static QString colorShader(const Video::ImageFormat& d)
  {
    QString shader;
    shader.reserve(1024);
    switch(d.output_format)
    {
      case Video::OutputFormat::Passthrough:
        // The X'Y'Z' code values, untouched.
        shader += "vec4 convert_to_rgb(vec4 tex) { return tex; }\n";
        break;

      case Video::OutputFormat::Linear:
        // Linear BT.2020, 1.0 = reference white: the wide-gamut convention of
        // the other decoders, and wide enough to hold DCI-P3 masters.
        shader += dcdm_eotf;
        shader += xyz_to_bt2020;
        shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  return vec4(xyzToRgb * dcdmToXYZ(tex.xyz), 1.0);
}
)_";
        break;

      case Video::OutputFormat::Normalized:
        // Linear BT.2020, 1.0 = the code-value peak (52.37 cd/m^2).
        shader += dcdm_eotf;
        shader += xyz_to_bt2020;
        shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  return vec4(xyzToRgb * dcdmToXYZ(tex.xyz) * (48.0 / 52.37), 1.0);
}
)_";
        break;

      case Video::OutputFormat::SDR:
      default:
        // sRGB, with the reference white at 1.0. Out-of-gamut and above-white
        // values clip, as a DCI-P3 master on a BT.709 display must.
        shader += dcdm_eotf;
        shader += xyz_to_bt709;
        shader += SRGB_OETF;
        shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  vec3 rgb = clamp(xyzToRgb * dcdmToXYZ(tex.xyz), 0.0, 1.0);
  return vec4(srgbOetf(rgb), 1.0);
}
)_";
        break;
    }
    return shader;
  }

  explicit XYZ12Decoder(Video::ImageFormat& d, QString f = "")
      : decoder{d}
      , filter{std::move(f)}
  {
  }

  Video::ImageFormat& decoder;
  QString filter;

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    {
      auto tex = rhi.newTexture(QRhiTexture::R16, QSize{w * 3, h}, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();

      samplers.push_back({sampler, tex});
    }

    return score::gfx::makeShaders(
        r.state, vertexShader(),
        QString(frag).arg(filter).arg(colorShader(decoder)));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    const auto w = decoder.width, h = decoder.height;

    // 3 R16 samples per pixel = 6 bytes per pixel
    QRhiTextureUploadEntry entry{
        0, 0, createTextureUpload(frame.data[0], w * 3, h, 2, frame.linesize[0])};

    QRhiTextureUploadDescription desc{entry};
    res.uploadTexture(samplers[0].texture, desc);
  }
};

}

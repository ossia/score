#pragma once

/**
 * @file ColorSpaceOut.hpp
 * @brief GPU shader generation for RGB->YUV color space conversion (output/encoding).
 *
 * This is the inverse of ColorSpace.hpp (which handles YUV->RGB for decoding).
 * It generates GLSL shader strings that define a `vec3 convert_from_rgb(vec3 rgb)`
 * function, which converts from the internal rendering colorspace (sRGB RGBA)
 * to the target output YUV colorspace.
 *
 * Matrices are baked into the shader at compile time as `const mat4` values,
 * following the same pattern as the decoder's `convert_to_rgb()`.
 *
 * The 4th row of each mat4 encodes the YUV offset (e.g., 0.0/0.5/0.5 for full range,
 * 16/128/128 scaled for limited range).
 *
 * Usage:
 *   QString shader = colorMatrixOut(AVCOL_SPC_BT709, AVCOL_TRC_BT709,
 *                                    AVCOL_RANGE_JPEG, AVCOL_PRI_BT709);
 *   // shader defines: vec3 convert_from_rgb(vec3 rgb) { ... }
 *   // Insert into fragment shader via QString::arg()
 */

extern "C" {
#include <libavutil/pixfmt.h>
}

#include <QString>

namespace score::gfx
{

/**
 * @brief The input transfer function is described using AVColorTransferCharacteristic.
 *
 * This tells the encoder what transfer function the RGBA texture contains:
 *   - AVCOL_TRC_IEC61966_2_1 (sRGB) — default, from SDR decode
 *   - AVCOL_TRC_LINEAR — from Linear/Normalized decode
 *   - AVCOL_TRC_SMPTE2084 (PQ) — HDR10 passthrough
 *   - AVCOL_TRC_ARIB_STD_B67 (HLG) — HLG passthrough
 *   - AVCOL_TRC_UNSPECIFIED — passthrough, skip all transfer function conversion
 *
 * The default input transfer is sRGB (AVCOL_TRC_IEC61966_2_1).
 */
static constexpr AVColorTransferCharacteristic DefaultInputTrc = AVCOL_TRC_IEC61966_2_1;

/// Color matrices

// Column-major mat4 (GLSL order). The 4th column is the YUV offset.
// Each matrix converts linear/gamma RGB to Y'CbCr.

// BT.601 full range (JPEG): Y=[0,255] Cb,Cr=[0,255] centered at 128
#define SCORE_GFX_RGB_TO_BT601_FULL_MATRIX \
  "mat4(\n\
     0.299,    -0.168736,  0.5,      0.0,\n\
     0.587,    -0.331264, -0.418688, 0.0,\n\
     0.114,     0.5,      -0.081312, 0.0,\n\
     0.0,       0.5,       0.5,      1.0)\n"

// BT.601 limited range (MPEG): Y=[16,235] Cb,Cr=[16,240]
#define SCORE_GFX_RGB_TO_BT601_LIMITED_MATRIX \
  "mat4(\n\
     0.256788,  -0.148223,  0.439216, 0.0,\n\
     0.504129,  -0.290993, -0.367788, 0.0,\n\
     0.097906,   0.439216, -0.071427, 0.0,\n\
     0.062745,   0.501961,  0.501961, 1.0)\n"

// BT.709 full range: Y=[0,255] Cb,Cr=[0,255] centered at 128
#define SCORE_GFX_RGB_TO_BT709_FULL_MATRIX \
  "mat4(\n\
     0.2126,   -0.114572,  0.5,      0.0,\n\
     0.7152,   -0.385428, -0.454153, 0.0,\n\
     0.0722,    0.5,      -0.045847, 0.0,\n\
     0.0,       0.5,       0.5,      1.0)\n"

// BT.709 limited range: Y=[16,235] Cb,Cr=[16,240]
#define SCORE_GFX_RGB_TO_BT709_LIMITED_MATRIX \
  "mat4(\n\
     0.182586,  -0.100644,  0.439216, 0.0,\n\
     0.614231,  -0.338572, -0.398942, 0.0,\n\
     0.062007,   0.439216, -0.040274, 0.0,\n\
     0.062745,   0.501961,  0.501961, 1.0)\n"

// BT.2020 full range
#define SCORE_GFX_RGB_TO_BT2020_FULL_MATRIX \
  "mat4(\n\
     0.2627,   -0.139630,  0.5,      0.0,\n\
     0.6780,   -0.360370, -0.459786, 0.0,\n\
     0.0593,    0.5,      -0.040214, 0.0,\n\
     0.0,       0.5,       0.5,      1.0)\n"

// BT.2020 limited range
#define SCORE_GFX_RGB_TO_BT2020_LIMITED_MATRIX \
  "mat4(\n\
     0.225613,  -0.122655,  0.439216, 0.0,\n\
     0.582282,  -0.316560, -0.403890, 0.0,\n\
     0.050928,   0.439216, -0.035326, 0.0,\n\
     0.062745,   0.501961,  0.501961, 1.0)\n"

#define SCORE_GFX_RGB_TO_SMPTE240M_FULL_MATRIX \
  "mat4(\n\
     0.2122,   -0.115765,  0.5,      0.0,\n\
     0.7013,   -0.382235, -0.445418, 0.0,\n\
     0.0865,    0.5,      -0.054582, 0.0,\n\
     0.0,       0.5,       0.5,      1.0)\n"

// Each defines: vec3 convert_from_rgb(vec3 rgb)

#define SCORE_GFX_CONVERT_FROM_RGB_TO_BT601_FULL                           \
  "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_BT601_FULL_MATRIX ";\n"   \
  "vec3 convert_from_rgb(vec3 rgb) { return (encode_matrix * vec4(rgb, 1.0)).xyz; }\n"

#define SCORE_GFX_CONVERT_FROM_RGB_TO_BT601_LIMITED                        \
  "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_BT601_LIMITED_MATRIX ";\n"\
  "vec3 convert_from_rgb(vec3 rgb) { return (encode_matrix * vec4(rgb, 1.0)).xyz; }\n"

#define SCORE_GFX_CONVERT_FROM_RGB_TO_BT709_FULL                           \
  "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_BT709_FULL_MATRIX ";\n"   \
  "vec3 convert_from_rgb(vec3 rgb) { return (encode_matrix * vec4(rgb, 1.0)).xyz; }\n"

#define SCORE_GFX_CONVERT_FROM_RGB_TO_BT709_LIMITED                        \
  "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_BT709_LIMITED_MATRIX ";\n"\
  "vec3 convert_from_rgb(vec3 rgb) { return (encode_matrix * vec4(rgb, 1.0)).xyz; }\n"

#define SCORE_GFX_CONVERT_FROM_RGB_TO_BT2020_FULL                          \
  "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_BT2020_FULL_MATRIX ";\n"  \
  "vec3 convert_from_rgb(vec3 rgb) { return (encode_matrix * vec4(rgb, 1.0)).xyz; }\n"

#define SCORE_GFX_CONVERT_FROM_RGB_TO_BT2020_LIMITED                       \
  "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_BT2020_LIMITED_MATRIX ";\n"\
  "vec3 convert_from_rgb(vec3 rgb) { return (encode_matrix * vec4(rgb, 1.0)).xyz; }\n"

/// inverse OETF / EOTF for output:

// PQ EOTF (PQ -> linear, inverse of PQ OETF, for decoding PQ-encoded input)
#define SCORE_GFX_PQ_EOTF R"_(
vec3 pqEotf(vec3 pq) {
  const float m1 = 0.1593017578125;
  const float m2 = 78.84375;
  const float c1 = 0.8359375;
  const float c2 = 18.8515625;
  const float c3 = 18.6875;
  vec3 Nm2 = pow(pq, vec3(1.0 / m2));
  return pow(max(Nm2 - c1, 0.0) / (c2 - c3 * Nm2), vec3(1.0 / m1));
}
)_"

// HLG EOTF (HLG -> linear, inverse of HLG OETF)
#define SCORE_GFX_HLG_EOTF R"_(
vec3 hlgEotf(vec3 hlg) {
  const float a = 0.17883277;
  const float b = 0.28466892;
  const float c = 0.55991073;
  vec3 lo = hlg * hlg / 3.0;
  vec3 hi = (exp((hlg - c) / a) + b) / 12.0;
  return mix(lo, hi, step(vec3(0.5), hlg));
}
)_"

// sRGB -> linear (inverse sRGB OETF, used before gamut conversion for HDR output)
#define SCORE_GFX_INVERSE_SRGB_OETF R"_(
vec3 inverseSrgbOetf(vec3 c) {
  vec3 lo = c / 12.92;
  vec3 hi = pow((c + 0.055) / 1.055, vec3(2.4));
  return mix(lo, hi, step(vec3(0.04045), c));
}
)_"

// Linear -> PQ (SMPTE ST 2084 OETF, for HDR10 output)
// Input: linear light in [0, 1] normalized to SDR peak (1.0 = sdrPeakNits)
// Output: PQ-encoded [0, 1]
#define SCORE_GFX_PQ_OETF R"_(
vec3 pqOetf(vec3 linearRgb) {
  const float m1 = 0.1593017578125;
  const float m2 = 78.84375;
  const float c1 = 0.8359375;
  const float c2 = 18.8515625;
  const float c3 = 18.6875;
  vec3 Ym1 = pow(linearRgb, vec3(m1));
  return pow((c1 + c2 * Ym1) / (1.0 + c3 * Ym1), vec3(m2));
}
)_"

// Linear -> HLG (ARIB STD-B67 OETF, for HLG output)
// Input: linear light in [0, 1] (scene-referred)
// Output: HLG-encoded [0, 1]
#define SCORE_GFX_HLG_OETF R"_(
vec3 hlgOetf(vec3 linearRgb) {
  const float a = 0.17883277;
  const float b = 0.28466892;  // 1.0 - 4.0 * a
  const float c = 0.55991073;  // 0.5 - a * ln(4.0 * a)
  vec3 lo = sqrt(3.0 * linearRgb);
  vec3 hi = a * log(12.0 * linearRgb - b) + c;
  return mix(lo, hi, step(vec3(1.0 / 12.0), linearRgb));
}
)_"

// Linear -> BT.709 gamma (approximate 0.45 power)
#define SCORE_GFX_BT709_OETF R"_(
vec3 bt709Oetf(vec3 linearRgb) {
  vec3 lo = 4.5 * linearRgb;
  vec3 hi = 1.099 * pow(linearRgb, vec3(0.45)) - 0.099;
  return mix(lo, hi, step(vec3(0.018), linearRgb));
}
)_"

// sRGB OETF (linear -> sRGB gamma)
#define SCORE_GFX_SRGB_OETF_OUT R"_(
vec3 srgbOetfOut(vec3 linearRgb) {
  vec3 lo = 12.92 * linearRgb;
  vec3 hi = 1.055 * pow(linearRgb, vec3(1.0 / 2.4)) - 0.055;
  return mix(lo, hi, step(vec3(0.0031308), linearRgb));
}
)_"

/// Gamut conversion

// BT.709 -> BT.2020 (for SDR->HDR gamut expansion)
#define SCORE_GFX_BT709_TO_BT2020_GAMUT R"_(
const mat3 gamutConvertOut = mat3(
  0.6274,  0.0691,  0.0164,
  0.3293,  0.9195,  0.0880,
  0.0433,  0.0114,  0.8956
);
)_"

// BT.709 -> Display P3
#define SCORE_GFX_BT709_TO_P3_GAMUT R"_(
const mat3 gamutConvertOut = mat3(
  0.8225,  0.0332,  0.0171,
  0.1774,  0.9669,  0.0724,
  0.0001,  -0.0001, 0.9106
);
)_"

// Builds an SDR output shader with the given matrix macro.
// Handles all input transfer functions for SDR YUV output.
static inline QString sdrOutShader(
    const char* matrix, AVColorTransferCharacteristic input_trc)
{
  QString shader;
  shader.reserve(2048);

  if(input_trc == AVCOL_TRC_LINEAR)
  {
    // Linear -> sRGB gamma -> YUV matrix
    shader += SCORE_GFX_SRGB_OETF_OUT;
    shader += matrix;
    shader += "vec3 convert_from_rgb(vec3 rgb) { return (encode_matrix * vec4(srgbOetfOut(rgb), 1.0)).xyz; }\n";
  }
  else if(input_trc == AVCOL_TRC_SMPTE2084)
  {
    // PQ -> linear -> sRGB gamma -> YUV matrix
    shader += SCORE_GFX_PQ_EOTF;
    shader += SCORE_GFX_SRGB_OETF_OUT;
    shader += matrix;
    shader += "vec3 convert_from_rgb(vec3 rgb) { return (encode_matrix * vec4(srgbOetfOut(pqEotf(rgb)), 1.0)).xyz; }\n";
  }
  else if(input_trc == AVCOL_TRC_ARIB_STD_B67)
  {
    // HLG -> linear -> sRGB gamma -> YUV matrix
    shader += SCORE_GFX_HLG_EOTF;
    shader += SCORE_GFX_SRGB_OETF_OUT;
    shader += matrix;
    shader += "vec3 convert_from_rgb(vec3 rgb) { return (encode_matrix * vec4(srgbOetfOut(hlgEotf(rgb)), 1.0)).xyz; }\n";
  }
  else
  {
    // sRGB / passthrough / unspecified -> direct matrix multiply
    shader += matrix;
    shader += "vec3 convert_from_rgb(vec3 rgb) { return (encode_matrix * vec4(rgb, 1.0)).xyz; }\n";
  }
  return shader;
}

// SDR BT.709 output
static inline QString bt709OutShader(
    AVColorRange range,
    AVColorTransferCharacteristic input_trc = DefaultInputTrc)
{
  const char* matrix = (range == AVCOL_RANGE_JPEG)
      ? "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_BT709_FULL_MATRIX ";\n"
      : "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_BT709_LIMITED_MATRIX ";\n";
  return sdrOutShader(matrix, input_trc);
}

// SDR BT.601 output
static inline QString bt601OutShader(
    AVColorRange range,
    AVColorTransferCharacteristic input_trc = DefaultInputTrc)
{
  const char* matrix = (range == AVCOL_RANGE_JPEG)
      ? "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_BT601_FULL_MATRIX ";\n"
      : "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_BT601_LIMITED_MATRIX ";\n";
  return sdrOutShader(matrix, input_trc);
}

// HDR BT.2020 + PQ output (HDR10)
static inline QString bt2020PqOutShader(
    AVColorRange range,
    AVColorTransferCharacteristic input_trc = DefaultInputTrc)
{
  QString shader;
  shader.reserve(4096);

  const char* matrix = (range == AVCOL_RANGE_JPEG)
      ? "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_BT2020_FULL_MATRIX ";\n"
      : "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_BT2020_LIMITED_MATRIX ";\n";

  if(input_trc == AVCOL_TRC_UNSPECIFIED)
  {
    // Passthrough — already encoded, just matrix
    shader += matrix;
    shader += "vec3 convert_from_rgb(vec3 rgb) { return (encode_matrix * vec4(rgb, 1.0)).xyz; }\n";
  }
  else if(input_trc == AVCOL_TRC_SMPTE2084)
  {
    // PQ BT.709 -> linear -> gamut -> PQ
    shader += SCORE_GFX_PQ_EOTF;
    shader += SCORE_GFX_BT709_TO_BT2020_GAMUT;
    shader += SCORE_GFX_PQ_OETF;
    shader += matrix;
    shader += R"_(
vec3 convert_from_rgb(vec3 rgb) {
  vec3 linearBt709 = pqEotf(rgb);
  vec3 linearBt2020 = gamutConvertOut * linearBt709;
  return (encode_matrix * vec4(pqOetf(linearBt2020), 1.0)).xyz;
}
)_";
  }
  else if(input_trc == AVCOL_TRC_ARIB_STD_B67)
  {
    // HLG BT.709 -> linear -> gamut -> PQ
    shader += SCORE_GFX_HLG_EOTF;
    shader += SCORE_GFX_BT709_TO_BT2020_GAMUT;
    shader += SCORE_GFX_PQ_OETF;
    shader += matrix;
    shader += R"_(
vec3 convert_from_rgb(vec3 rgb) {
  vec3 linearBt709 = hlgEotf(rgb);
  vec3 linearBt2020 = gamutConvertOut * linearBt709;
  return (encode_matrix * vec4(pqOetf(linearBt2020), 1.0)).xyz;
}
)_";
  }
  else if(input_trc == AVCOL_TRC_LINEAR)
  {
    // Linear BT.709 -> gamut -> PQ
    shader += SCORE_GFX_BT709_TO_BT2020_GAMUT;
    shader += SCORE_GFX_PQ_OETF;
    shader += matrix;
    shader += R"_(
vec3 convert_from_rgb(vec3 rgb) {
  vec3 linearBt2020 = gamutConvertOut * rgb;
  return (encode_matrix * vec4(pqOetf(linearBt2020), 1.0)).xyz;
}
)_";
  }
  else
  {
    // sRGB (default) -> linear -> gamut -> PQ
    shader += SCORE_GFX_INVERSE_SRGB_OETF;
    shader += SCORE_GFX_BT709_TO_BT2020_GAMUT;
    shader += SCORE_GFX_PQ_OETF;
    shader += matrix;
    shader += R"_(
vec3 convert_from_rgb(vec3 rgb) {
  vec3 linearBt709 = inverseSrgbOetf(rgb);
  vec3 linearBt2020 = gamutConvertOut * linearBt709;
  return (encode_matrix * vec4(pqOetf(linearBt2020), 1.0)).xyz;
}
)_";
  }
  return shader;
}

// HDR BT.2020 + HLG output
static inline QString bt2020HlgOutShader(
    AVColorRange range,
    AVColorTransferCharacteristic input_trc = DefaultInputTrc)
{
  QString shader;
  shader.reserve(4096);

  const char* matrix = (range == AVCOL_RANGE_JPEG)
      ? "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_BT2020_FULL_MATRIX ";\n"
      : "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_BT2020_LIMITED_MATRIX ";\n";

  if(input_trc == AVCOL_TRC_UNSPECIFIED)
  {
    shader += matrix;
    shader += "vec3 convert_from_rgb(vec3 rgb) { return (encode_matrix * vec4(rgb, 1.0)).xyz; }\n";
  }
  else if(input_trc == AVCOL_TRC_SMPTE2084)
  {
    shader += SCORE_GFX_PQ_EOTF;
    shader += SCORE_GFX_BT709_TO_BT2020_GAMUT;
    shader += SCORE_GFX_HLG_OETF;
    shader += matrix;
    shader += R"_(
vec3 convert_from_rgb(vec3 rgb) {
  vec3 linearBt709 = pqEotf(rgb);
  vec3 linearBt2020 = gamutConvertOut * linearBt709;
  return (encode_matrix * vec4(hlgOetf(linearBt2020), 1.0)).xyz;
}
)_";
  }
  else if(input_trc == AVCOL_TRC_ARIB_STD_B67)
  {
    shader += SCORE_GFX_HLG_EOTF;
    shader += SCORE_GFX_BT709_TO_BT2020_GAMUT;
    shader += SCORE_GFX_HLG_OETF;
    shader += matrix;
    shader += R"_(
vec3 convert_from_rgb(vec3 rgb) {
  vec3 linearBt709 = hlgEotf(rgb);
  vec3 linearBt2020 = gamutConvertOut * linearBt709;
  return (encode_matrix * vec4(hlgOetf(linearBt2020), 1.0)).xyz;
}
)_";
  }
  else if(input_trc == AVCOL_TRC_LINEAR)
  {
    shader += SCORE_GFX_BT709_TO_BT2020_GAMUT;
    shader += SCORE_GFX_HLG_OETF;
    shader += matrix;
    shader += R"_(
vec3 convert_from_rgb(vec3 rgb) {
  vec3 linearBt2020 = gamutConvertOut * rgb;
  return (encode_matrix * vec4(hlgOetf(linearBt2020), 1.0)).xyz;
}
)_";
  }
  else
  {
    shader += SCORE_GFX_INVERSE_SRGB_OETF;
    shader += SCORE_GFX_BT709_TO_BT2020_GAMUT;
    shader += SCORE_GFX_HLG_OETF;
    shader += matrix;
    shader += R"_(
vec3 convert_from_rgb(vec3 rgb) {
  vec3 linearBt709 = inverseSrgbOetf(rgb);
  vec3 linearBt2020 = gamutConvertOut * linearBt709;
  return (encode_matrix * vec4(hlgOetf(linearBt2020), 1.0)).xyz;
}
)_";
  }
  return shader;
}

// SDR BT.2020 output (wide gamut SDR)
static inline QString bt2020SdrOutShader(
    AVColorRange range,
    AVColorTransferCharacteristic input_trc = DefaultInputTrc)
{
  QString shader;
  shader.reserve(4096);

  const char* matrix = (range == AVCOL_RANGE_JPEG)
      ? "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_BT2020_FULL_MATRIX ";\n"
      : "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_BT2020_LIMITED_MATRIX ";\n";

  if(input_trc == AVCOL_TRC_UNSPECIFIED)
  {
    shader += matrix;
    shader += "vec3 convert_from_rgb(vec3 rgb) { return (encode_matrix * vec4(rgb, 1.0)).xyz; }\n";
  }
  else if(input_trc == AVCOL_TRC_LINEAR)
  {
    shader += SCORE_GFX_BT709_TO_BT2020_GAMUT;
    shader += SCORE_GFX_SRGB_OETF_OUT;
    shader += matrix;
    shader += R"_(
vec3 convert_from_rgb(vec3 rgb) {
  vec3 linearBt2020 = gamutConvertOut * rgb;
  return (encode_matrix * vec4(srgbOetfOut(linearBt2020), 1.0)).xyz;
}
)_";
  }
  else
  {
    // sRGB default
    shader += SCORE_GFX_INVERSE_SRGB_OETF;
    shader += SCORE_GFX_BT709_TO_BT2020_GAMUT;
    shader += SCORE_GFX_SRGB_OETF_OUT;
    shader += matrix;
    shader += R"_(
vec3 convert_from_rgb(vec3 rgb) {
  vec3 linearBt709 = inverseSrgbOetf(rgb);
  vec3 linearBt2020 = gamutConvertOut * linearBt709;
  return (encode_matrix * vec4(srgbOetfOut(linearBt2020), 1.0)).xyz;
}
)_";
  }
  return shader;
}

/**
 * @brief Returns GLSL code defining `vec3 convert_from_rgb(vec3 rgb)`.
 *
 * The function converts from the internal rendering colorspace to the target
 * YUV encoding. The returned string should be inserted into the fragment
 * shader via %1 placeholder.
 *
 * @param color_space     AVColorSpace (AVCOL_SPC_*)
 * @param color_trc       AVColorTransferCharacteristic (AVCOL_TRC_*)
 * @param color_range     AVColorRange (AVCOL_RANGE_MPEG or AVCOL_RANGE_JPEG)
 * @param color_primaries AVColorPrimaries (AVCOL_PRI_*)
 * @param input           What transfer function the input texture uses.
 *                        Must match the Video::OutputFormat on the decode side.
 */
static inline QString colorMatrixOut(
    AVColorSpace color_space,
    AVColorTransferCharacteristic color_trc,
    AVColorRange color_range,
    AVColorPrimaries color_primaries,
    AVColorTransferCharacteristic input_trc = DefaultInputTrc)
{
  switch(color_space)
  {
    case AVCOL_SPC_RGB:
      return "vec3 convert_from_rgb(vec3 rgb) { return rgb; }\n";

    case AVCOL_SPC_BT709:
      return bt709OutShader(color_range, input_trc);

    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
      return bt601OutShader(color_range, input_trc);

    case AVCOL_SPC_SMPTE240M:
    {
      const bool full = (color_range == AVCOL_RANGE_JPEG);
      if(full)
      {
        QString shader;
        shader += "const mat4 encode_matrix = " SCORE_GFX_RGB_TO_SMPTE240M_FULL_MATRIX ";\n";
        shader += "vec3 convert_from_rgb(vec3 rgb) { return (encode_matrix * vec4(rgb, 1.0)).xyz; }\n";
        return shader;
      }
      return bt709OutShader(color_range, input_trc);
    }

    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
      if(color_trc == AVCOL_TRC_SMPTE2084)
        return bt2020PqOutShader(color_range, input_trc);
      else if(color_trc == AVCOL_TRC_ARIB_STD_B67)
        return bt2020HlgOutShader(color_range, input_trc);
      else
        return bt2020SdrOutShader(color_range, input_trc);

    default:
      break;
  }

  // Fallback based on primaries
  if(color_primaries == AVCOL_PRI_BT2020)
  {
    if(color_trc == AVCOL_TRC_SMPTE2084)
      return bt2020PqOutShader(color_range, input_trc);
    else if(color_trc == AVCOL_TRC_ARIB_STD_B67)
      return bt2020HlgOutShader(color_range, input_trc);
    else
      return bt2020SdrOutShader(color_range, input_trc);
  }

  // Ultimate fallback: BT.709
  return bt709OutShader(color_range, input_trc);
}

static inline QString colorMatrixOut()
{
  return colorMatrixOut(
      AVCOL_SPC_BT709, AVCOL_TRC_BT709, AVCOL_RANGE_JPEG, AVCOL_PRI_BT709,
      DefaultInputTrc);
}

}
#pragma once
#include <Video/VideoInterface.hpp>
#include <Gfx/Graph/decoders/Tonemap.hpp>

// See softpixel.com/~cwright/programming/colorspace/yuv
//
// https://github.com/vlc-qt/vlc-qt/blob/master/src/qml/painter/GlPainter.cpp#L48
//
// All mat4 matrices are in column-major (GLSL) order.
// Input: vec4(Y, Cb, Cr, 1.0) with values in [0, 1] normalized range.
// The 4th column encodes the offset.
//
// Limited range matrices expect:
//   Y in [16/255, 235/255], Cb/Cr in [16/255, 240/255]
//   Y is scaled by 255/219, Cb/Cr by 255/224
//
// Full range matrices expect:
//   Y in [0, 1], Cb/Cr in [0, 1] (centered at 0.5)
//
// Kr/Kb coefficients used:
//   BT.601:     Kr=0.299,  Kb=0.114
//   BT.709:     Kr=0.2126, Kb=0.0722
//   SMPTE 240M: Kr=0.2122, Kb=0.0865
//   FCC:        Kr=0.30,   Kb=0.11
//   BT.2020:    Kr=0.2627, Kb=0.0593

namespace score::gfx
{

// ============================================================
//  Identity (RGB passthrough)
// ============================================================

#define SCORE_GFX_RGB_MATRIX \
  "mat4(\
    1., 0., 0., 0.0,\n\
    0., 1., 0., 0.0,\n\
    0., 0., 1., 0.0,\n\
    0., 0., 0., 1.0)\n"

// ============================================================
//  BT.601 (Kr=0.299, Kb=0.114)
// ============================================================

#define SCORE_GFX_BT601_LIMITED_MATRIX \
  "mat4(\n\
    1.164383561643836,  1.164383561643836,  1.164383561643836, 0.0,\n\
    0.000000000000000, -0.391762290094914,  2.017232142857143, 0.0,\n\
    1.596026785714286, -0.812967647237771,  0.000000000000000, 0.0,\n\
   -0.874202217873451,  0.531667823499146, -1.085630789302022, 1.0)\n"

#define SCORE_GFX_BT601_FULL_MATRIX \
  "mat4(\n\
    1.000000000000000,  1.000000000000000,  1.000000000000000, 0.0,\n\
    0.000000000000000, -0.344136286201022,  1.772000000000000, 0.0,\n\
    1.402000000000000, -0.714136286201022,  0.000000000000000, 0.0,\n\
   -0.701000000000000,  0.529136286201022, -0.886000000000000, 1.0)\n"

// Backward compat aliases
#define SCORE_GFX_BT601_MATRIX SCORE_GFX_BT601_LIMITED_MATRIX

// ============================================================
//  BT.709 (Kr=0.2126, Kb=0.0722)
// ============================================================

#define SCORE_GFX_BT709_LIMITED_MATRIX \
  "mat4(\n\
    1.164383561643836,  1.164383561643836,  1.164383561643836, 0.0,\n\
    0.000000000000000, -0.213248614273730,  2.112401785714286, 0.0,\n\
    1.792741071428571, -0.532909328559444,  0.000000000000000, 0.0,\n\
   -0.972945075016308,  0.301482665475862, -1.133402217873451, 1.0)\n"

#define SCORE_GFX_BT709_FULL_MATRIX \
  "mat4(\n\
    1.000000000000000,  1.000000000000000,  1.000000000000000, 0.0,\n\
    0.000000000000000, -0.187324272930649,  1.855600000000000, 0.0,\n\
    1.574800000000000, -0.468124272930649,  0.000000000000000, 0.0,\n\
   -0.787400000000000,  0.327724272930649, -0.927800000000000, 1.0)\n"

// Backward compat aliases
#define SCORE_GFX_BT709_MATRIX SCORE_GFX_BT709_LIMITED_MATRIX

// ============================================================
//  SMPTE 240M (Kr=0.2122, Kb=0.0865)
// ============================================================

#define SCORE_GFX_SMPTE240M_LIMITED_MATRIX \
  "mat4(\n\
    1.164383561643836,  1.164383561643836,  1.164383561643836, 0.0,\n\
    0.000000000000000, -0.256532845251675,  2.079843750000000, 0.0,\n\
    1.793651785714286, -0.542724809537390,  0.000000000000000, 0.0,\n\
   -0.973402217873451,  0.328136638536074, -1.117059360730594, 1.0)\n"

#define SCORE_GFX_SMPTE240M_FULL_MATRIX \
  "mat4(\n\
    1.000000000000000,  1.000000000000000,  1.000000000000000, 0.0,\n\
    0.000000000000000, -0.225346499358335,  1.827000000000000, 0.0,\n\
    1.575600000000000, -0.476746499358335,  0.000000000000000, 0.0,\n\
   -0.787800000000000,  0.351046499358335, -0.913500000000000, 1.0)\n"

// ============================================================
//  FCC (Kr=0.30, Kb=0.11)
// ============================================================

#define SCORE_GFX_FCC_LIMITED_MATRIX \
  "mat4(\n\
    1.164383561643836,  1.164383561643836,  1.164383561643836, 0.0,\n\
    0.000000000000000, -0.377792070217918,  2.026339285714286, 0.0,\n\
    1.593750000000000, -0.810381355932203,  0.000000000000000, 0.0,\n\
   -0.873059360730594,  0.523357104160448, -1.090202217873451, 1.0)\n"

#define SCORE_GFX_FCC_FULL_MATRIX \
  "mat4(\n\
    1.000000000000000,  1.000000000000000,  1.000000000000000, 0.0,\n\
    0.000000000000000, -0.331864406779661,  1.780000000000000, 0.0,\n\
    1.400000000000000, -0.711864406779661,  0.000000000000000, 0.0,\n\
   -0.700000000000000,  0.521864406779661, -0.890000000000000, 1.0)\n"

// ============================================================
//  YCgCo
//  R = Y - Cg + Co,  G = Y + Cg,  B = Y - Cg - Co
//  (Cg and Co stored centered at 0.5)
// ============================================================

#define SCORE_GFX_YCGCO_LIMITED_MATRIX \
  "mat4(\n\
    1.164383561643836,  1.164383561643836,  1.164383561643836, 0.0,\n\
   -1.138392857142857,  1.138392857142857, -1.138392857142857, 0.0,\n\
    1.138392857142857,  0.000000000000000, -1.138392857142857, 0.0,\n\
   -0.073059360730594, -0.644487932159165,  1.069797782126549, 1.0)\n"

#define SCORE_GFX_YCGCO_FULL_MATRIX \
  "mat4(\n\
    1.0,  1.0,  1.0, 0.0,\n\
   -1.0,  1.0, -1.0, 0.0,\n\
    1.0,  0.0, -1.0, 0.0,\n\
    0.0, -0.5,  0.5, 1.0)\n"

// ============================================================
//  BT.2020 NCL (Kr=0.2627, Kb=0.0593)
//  Used by the BT.2020 HDR pipeline for the initial YUV->RGB step.
//  The full HDR path (EOTF, tonemap, gamut, OETF) is in bt2020shader().
// ============================================================

#define SCORE_GFX_BT2020_LIMITED_MATRIX \
  "mat4(\n\
    1.164383561643836,  1.164383561643836,  1.164383561643836, 0.0,\n\
    0.000000000000000, -0.187326104219343,  2.141772321428571, 0.0,\n\
    1.678674107142857, -0.650424318505057,  0.000000000000000, 0.0,\n\
   -0.915687932159165,  0.347458498519301, -1.148145075016308, 1.0)\n"

#define SCORE_GFX_BT2020_FULL_MATRIX \
  "mat4(\n\
    1.000000000000000,  1.000000000000000,  1.000000000000000, 0.0,\n\
    0.000000000000000, -0.164553126843658,  1.881400000000000, 0.0,\n\
    1.474600000000000, -0.571353126843658,  0.000000000000000, 0.0,\n\
   -0.737300000000000,  0.367953126843658, -0.940700000000000, 1.0)\n"

// ============================================================
//  Convenience macros for convert_to_rgb() function generation
// ============================================================

// --- BT.601 ---
#define SCORE_GFX_CONVERT_BT601_LIMITED_TO_RGB                              \
  "const mat4 conversion_matrix = " SCORE_GFX_BT601_LIMITED_MATRIX ";\n"    \
  "vec4 convert_to_rgb(vec4 tex) { return conversion_matrix * tex; }\n"

#define SCORE_GFX_CONVERT_BT601_FULL_TO_RGB                                 \
  "const mat4 conversion_matrix = " SCORE_GFX_BT601_FULL_MATRIX ";\n"      \
  "vec4 convert_to_rgb(vec4 tex) { return conversion_matrix * tex; }\n"

// --- BT.709 ---
#define SCORE_GFX_CONVERT_BT709_LIMITED_TO_RGB                              \
  "const mat4 conversion_matrix = " SCORE_GFX_BT709_LIMITED_MATRIX ";\n"    \
  "vec4 convert_to_rgb(vec4 tex) { return conversion_matrix * tex; }\n"

#define SCORE_GFX_CONVERT_BT709_FULL_TO_RGB                                 \
  "const mat4 conversion_matrix = " SCORE_GFX_BT709_FULL_MATRIX ";\n"      \
  "vec4 convert_to_rgb(vec4 tex) { return conversion_matrix * tex; }\n"

// --- SMPTE 240M ---
#define SCORE_GFX_CONVERT_SMPTE240M_LIMITED_TO_RGB                          \
  "const mat4 conversion_matrix = " SCORE_GFX_SMPTE240M_LIMITED_MATRIX ";\n"\
  "vec4 convert_to_rgb(vec4 tex) { return conversion_matrix * tex; }\n"

#define SCORE_GFX_CONVERT_SMPTE240M_FULL_TO_RGB                             \
  "const mat4 conversion_matrix = " SCORE_GFX_SMPTE240M_FULL_MATRIX ";\n"   \
  "vec4 convert_to_rgb(vec4 tex) { return conversion_matrix * tex; }\n"

// --- FCC ---
#define SCORE_GFX_CONVERT_FCC_LIMITED_TO_RGB                                \
  "const mat4 conversion_matrix = " SCORE_GFX_FCC_LIMITED_MATRIX ";\n"      \
  "vec4 convert_to_rgb(vec4 tex) { return conversion_matrix * tex; }\n"

#define SCORE_GFX_CONVERT_FCC_FULL_TO_RGB                                   \
  "const mat4 conversion_matrix = " SCORE_GFX_FCC_FULL_MATRIX ";\n"         \
  "vec4 convert_to_rgb(vec4 tex) { return conversion_matrix * tex; }\n"

// --- YCgCo ---
#define SCORE_GFX_CONVERT_YCGCO_LIMITED_TO_RGB                              \
  "const mat4 conversion_matrix = " SCORE_GFX_YCGCO_LIMITED_MATRIX ";\n"    \
  "vec4 convert_to_rgb(vec4 tex) { return conversion_matrix * tex; }\n"

#define SCORE_GFX_CONVERT_YCGCO_FULL_TO_RGB                                 \
  "const mat4 conversion_matrix = " SCORE_GFX_YCGCO_FULL_MATRIX ";\n"      \
  "vec4 convert_to_rgb(vec4 tex) { return conversion_matrix * tex; }\n"

// Backward compat aliases
#define SCORE_GFX_CONVERT_BT601_TO_RGB SCORE_GFX_CONVERT_BT601_LIMITED_TO_RGB
#define SCORE_GFX_CONVERT_BT709_TO_RGB SCORE_GFX_CONVERT_BT709_LIMITED_TO_RGB

// ============================================================
//  BT.2020 gamut conversion matrix (for tone mapping pipeline)
//  Computed from first principles: BT.2020→XYZ→BT.709, D65 white.
//  Verified to 6 decimal places.
// ============================================================

#define SCORE_GFX_BT2020_TO_709_MATRIX \
  "mat4(\n\
    1.660491, -0.587641, -0.072850, 0.000000,\n\
    -0.124550, 1.132900, -0.008349, 0.000000,\n\
    -0.018151, -0.100579, 1.118730, 0.000000,\n\
    0.000000, 0.000000, 0.000000, 1.000000\n\
    )\n"

#define SCORE_GFX_BT2020_MATRIX SCORE_GFX_BT709_MATRIX

// ============================================================
//  BT.2020 HDR pipeline building blocks
//
//  These are reusable GLSL fragments for building the various
//  BT.2020 output modes (SDR, Passthrough, Linear, Normalized).
// ============================================================

// --- YUV to RGB matrices (mat3 + offset for BT.2020 NCL) ---

static constexpr auto BT2020_YUV_MATRIX_LIMITED = R"_(
const mat3 uYuvToRgbColorTransform = mat3(
  1.1689, 1.1689, 1.1689,
  0.0000, -0.1881, 2.1502,
  1.6853, -0.6530, 0.0000
);
const vec3 yuvOffset = vec3(0.0625, 0.5, 0.5);
)_";

static constexpr auto BT2020_YUV_MATRIX_FULL = R"_(
const mat3 uYuvToRgbColorTransform = mat3(
  1.0000, 1.0000, 1.0000,
  0.0000, -0.1646, 1.8814,
  1.4746, -0.5714, 0.0000
);
const vec3 yuvOffset = vec3(0.0, 0.5, 0.5);
)_";

// --- EOTF functions ---

static constexpr auto BT2020_PQ_EOTF = R"_(
vec3 applyEotf(vec3 v) {
  const float m1 = 2610.0 / 16384.0;
  const float m2 = (2523.0 / 4096.0) * 128.0;
  const float c1 = 3424.0 / 4096.0;
  const float c2 = (2413.0 / 4096.0) * 32.0;
  const float c3 = (2392.0 / 4096.0) * 32.0;
  vec3 p = pow(clamp(v, 0.0, 1.0), 1.0 / vec3(m2));
  return pow(max(p - c1, 0.0) / (c2 - c3 * p), 1.0 / vec3(m1));
}
)_";

static constexpr auto BT2020_HLG_EOTF = R"_(
float hlgEotfSingle(float v) {
  const float a = 0.17883277;
  const float b = 0.28466892;
  const float c = 0.55991073;
  return v <= 0.5 ? v * v / 3.0
                  : (b + exp((v - c) / a)) / 12.0;
}
vec3 applyEotf(vec3 v) {
  return vec3(hlgEotfSingle(v.r), hlgEotfSingle(v.g), hlgEotfSingle(v.b));
}
)_";

static constexpr auto BT2020_LINEAR_EOTF = R"_(
vec3 applyEotf(vec3 v) { return v; }
)_";

static constexpr auto BT2020_GAMMA22_EOTF = R"_(
vec3 applyEotf(vec3 v) { return pow(max(v, 0.0), vec3(2.2)); }
)_";

// --- Gamut conversion matrices ---

static constexpr auto BT2020_TO_BT709_GAMUT = R"_(
const mat3 gamutConvert = mat3(
   1.6605, -0.1246, -0.0182,
  -0.5876,  1.1329, -0.1006,
  -0.0728, -0.0083,  1.1187
);
)_";

// --- sRGB OETF ---

static constexpr auto SRGB_OETF = R"_(
vec3 srgbOetf(vec3 c) {
  vec3 lo = c * 12.92;
  vec3 hi = 1.055 * pow(max(c, 0.0), vec3(1.0 / 2.4)) - 0.055;
  return mix(lo, hi, step(vec3(0.0031308), c));
}
)_";

// ============================================================
//  BT.2020 shader generators for each OutputFormat
// ============================================================

//  Helper: emit YUV matrix based on range
static inline void bt2020_appendYuvMatrix(QString& shader, const Video::ImageFormat& d)
{
  if(d.color_range == AVCOL_RANGE_MPEG)
    shader += BT2020_YUV_MATRIX_LIMITED;
  else
    shader += BT2020_YUV_MATRIX_FULL;
}

// Helper: emit EOTF based on transfer characteristic
static inline void bt2020_appendEotf(QString& shader, const Video::ImageFormat& d)
{
  if(d.color_trc == AVCOL_TRC_SMPTE2084)
    shader += BT2020_PQ_EOTF;
  else if(d.color_trc == AVCOL_TRC_ARIB_STD_B67)
    shader += BT2020_HLG_EOTF;
  else if(d.color_trc == AVCOL_TRC_LINEAR)
    shader += BT2020_LINEAR_EOTF;
  else
    shader += BT2020_GAMMA22_EOTF;
}

// Helper: get content peak luminance in nits based on transfer function
static inline float bt2020_contentPeakNits(const Video::ImageFormat& d)
{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 3, 100)
#if __has_include(<libavutil/mastering_display_metadata.h>)
  if(d.content_light)
  {
    float cll = static_cast<float>(d.content_light->MaxCLL);
    if(cll >= 100.0f && cll <= 10000.0f)
      return cll;
  }

  if(d.mastering_display.has_luminance
     && d.mastering_display.max_luminance.den > 0)
  {
    float peak = static_cast<float>(av_q2d(d.mastering_display.max_luminance));
    if(peak >= 100.0f && peak <= 10000.0f)
      return peak;
  }
#endif
#endif

  // Defaults per transfer function
  if(d.color_trc == AVCOL_TRC_SMPTE2084)
    return 1000.0f;
  else if(d.color_trc == AVCOL_TRC_ARIB_STD_B67)
    return 1000.0f;
  else
    return 100.0f;
}

// Helper: for PQ, the EOTF outputs 1.0 = 10000 nits.
// This factor converts from EOTF output space to "1.0 = content peak" space.
static inline float bt2020_eotfToNormalizedFactor(const Video::ImageFormat& d)
{
  if(d.color_trc == AVCOL_TRC_SMPTE2084)
  {
    // PQ EOTF: 1.0 = 10000 nits. Content peak = 1000 nits (default).
    // So multiply by 10000/1000 = 10.0 to get 1.0 = content peak.
    return 10000.0f / bt2020_contentPeakNits(d);
  }
  else if(d.color_trc == AVCOL_TRC_ARIB_STD_B67)
  {
    // HLG EOTF: ~1.0 = reference white. Already roughly normalized.
    return 1.0f;
  }
  else
  {
    return 1.0f;
  }
}

// ──────────────────────────────────────────────────────────────
//  OutputFormat::Passthrough
//  YUV→RGB only. No EOTF. PQ values stay PQ-encoded, BT.2020 primaries.
//  For direct HDR10 swapchain output with no processing.
// ──────────────────────────────────────────────────────────────

static inline QString bt2020shader_passthrough(const Video::ImageFormat& d)
{
  QString shader;
  shader.reserve(1024);

  bt2020_appendYuvMatrix(shader, d);

  shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  vec3 rgb = clamp(uYuvToRgbColorTransform * (tex.xyz - yuvOffset), 0.0, 1.0);
  return vec4(rgb, 1.0);
}
)_";

  return shader;
}

// ──────────────────────────────────────────────────────────────
//  OutputFormat::Linear
//  YUV→RGB → EOTF → linear BT.2020.
//  PQ: 1.0 = 10000 nits. HLG: 1.0 ≈ reference white.
//  For HDR-aware compositing in the processing graph.
// ──────────────────────────────────────────────────────────────

static inline QString bt2020shader_linear(const Video::ImageFormat& d)
{
  QString shader;
  shader.reserve(2048);

  bt2020_appendYuvMatrix(shader, d);
  bt2020_appendEotf(shader, d);

  shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  vec3 rgb = clamp(uYuvToRgbColorTransform * (tex.xyz - yuvOffset), 0.0, 1.0);
  return vec4(applyEotf(rgb), 1.0);
}
)_";

  return shader;
}

// ──────────────────────────────────────────────────────────────
//  OutputFormat::Normalized
//  YUV→RGB → EOTF → divide by peak → linear BT.2020, 1.0 = content peak.
//  Friendlier for effects expecting 0–1 range.
// ──────────────────────────────────────────────────────────────

static inline QString bt2020shader_normalized(const Video::ImageFormat& d)
{
  QString shader;
  shader.reserve(2048);

  bt2020_appendYuvMatrix(shader, d);
  bt2020_appendEotf(shader, d);

  const float normFactor = bt2020_eotfToNormalizedFactor(d);
  shader += QString("const float eotfNormFactor = %1;\n").arg(normFactor, 0, 'f', 4);

  shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  vec3 rgb = clamp(uYuvToRgbColorTransform * (tex.xyz - yuvOffset), 0.0, 1.0);
  vec3 linear = applyEotf(rgb);
  // Normalize: 1.0 = content peak luminance
  return vec4(linear * eotfNormFactor, 1.0);
}
)_";

  return shader;
}

// ──────────────────────────────────────────────────────────────
//  OutputFormat::SDR
//  Full HDR→SDR pipeline, uses the selected Tonemap algorithm.
//
//  The gamut conversion order depends on the tonemapper type:
//
//  Luminance-based tonemappers (BT.2390, BT.2446, Reinhard):
//    Operate on a luminance channel and scale RGB proportionally.
//    Gamut-agnostic, so we tonemap in BT.2020 then convert:
//      YUV→RGB → EOTF → normalize → tonemap(BT.2020) → gamut → sRGB OETF
//
//  Per-channel tonemappers (ACES, AgX, Hable, PBR Neutral):
//    Apply curves per-channel or contain internal color-space
//    matrices that assume BT.709/sRGB input (ACES: sRGB→AP1,
//    AgX: sRGB→AgX log space). We must convert gamut first:
//      YUV→RGB → EOTF → normalize → gamut → tonemap(BT.709) → sRGB OETF
// ──────────────────────────────────────────────────────────────

static inline QString bt2020shader_sdr(const Video::ImageFormat& d)
{
  QString shader;
  shader.reserve(8192);

  // 1. YUV to RGB matrix
  bt2020_appendYuvMatrix(shader, d);

  // 2. EOTF
  bt2020_appendEotf(shader, d);

  // 3. Gamut conversion matrix (BT.2020 → BT.709)
  shader += BT2020_TO_BT709_GAMUT;

  // 4. sRGB OETF
  shader += SRGB_OETF;

  // 5. Normalization factor: EOTF output → "1.0 = content peak" space
  const float normFactor = bt2020_eotfToNormalizedFactor(d);
  shader += QString("const float eotfNormFactor = %1;\n").arg(normFactor, 0, 'f', 4);

  // 6. Tone mapping function: defines vec3 tonemap(vec3)
  //    with contentPeakNits and sdrPeakNits constants.
  const float contentPeak = bt2020_contentPeakNits(d);
  const float sdrPeak = 203.0f; // BT.2408 reference white
  shader += tonemapShader(d.tonemap, contentPeak, sdrPeak);

  // 7. convert_to_rgb: the complete pipeline
  //    Gamut conversion order depends on tonemapper type.
  const bool lumBased = isLuminanceBasedTonemap(d.tonemap);

  if(lumBased)
  {
    // Luminance-based: tonemap in BT.2020, then convert gamut
    shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  // Step 1: YUV decode → BT.2020 RGB
  vec3 rgb = clamp(uYuvToRgbColorTransform * (tex.xyz - yuvOffset), 0.0, 1.0);

  // Step 2: EOTF → linear light (BT.2020 primaries)
  vec3 linearBt2020 = applyEotf(rgb);

  // Step 3: Normalize so 1.0 = content peak
  linearBt2020 *= eotfNormFactor;

  // Step 4: Tone map in BT.2020 (luminance-based, gamut-agnostic)
  vec3 tonemapped = tonemap(linearBt2020);

  // Step 5: Gamut conversion BT.2020 → BT.709
  vec3 linearBt709 = gamutConvert * tonemapped;
  linearBt709 = clamp(linearBt709, 0.0, 1.0);

  // Step 6: sRGB OETF for display
  return vec4(srgbOetf(linearBt709), 1.0);
}
)_";
  }
  else
  {
    // Per-channel: convert gamut first, then tonemap in BT.709
    shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  // Step 1: YUV decode → BT.2020 RGB
  vec3 rgb = clamp(uYuvToRgbColorTransform * (tex.xyz - yuvOffset), 0.0, 1.0);

  // Step 2: EOTF → linear light (BT.2020 primaries)
  vec3 linearBt2020 = applyEotf(rgb);

  // Step 3: Normalize so 1.0 = content peak
  linearBt2020 *= eotfNormFactor;

  // Step 4: Gamut conversion BT.2020 → BT.709 (BEFORE tonemapping)
  vec3 linearBt709 = gamutConvert * linearBt2020;
  // Note: do NOT clamp here — values outside [0,1] represent
  // BT.2020-exclusive colors. The tonemapper will handle them.

  // Step 5: Tone map in BT.709 (per-channel / assumes BT.709 input)
  vec3 tonemapped = tonemap(linearBt709);

  // Step 6: Clamp and sRGB OETF for display
  return vec4(srgbOetf(clamp(tonemapped, 0.0, 1.0)), 1.0);
}
)_";
  }

  return shader;
}

// ============================================================
//  BT.2020 shader dispatch based on OutputFormat
// ============================================================

static inline QString bt2020shader(const Video::ImageFormat& d)
{
  switch(d.output_format)
  {
    case Video::OutputFormat::Passthrough:
      return bt2020shader_passthrough(d);
    case Video::OutputFormat::Linear:
      return bt2020shader_linear(d);
    case Video::OutputFormat::Normalized:
      return bt2020shader_normalized(d);
    case Video::OutputFormat::SDR:
    default:
      return bt2020shader_sdr(d);
  }
}

// ============================================================
//  Helper to select limited vs full range conversion macro
// ============================================================

static inline QString colorMatrix(const Video::ImageFormat& d)
{
  const bool full_range = (d.color_range == AVCOL_RANGE_JPEG);

  switch(d.color_space)
  {
    case AVCOL_SPC_RGB:
      return "vec4 convert_to_rgb(vec4 tex) { return tex; }";

    case AVCOL_SPC_BT709:
      return full_range ? SCORE_GFX_CONVERT_BT709_FULL_TO_RGB
                        : SCORE_GFX_CONVERT_BT709_LIMITED_TO_RGB;

    case AVCOL_SPC_FCC:
      return full_range ? SCORE_GFX_CONVERT_FCC_FULL_TO_RGB
                        : SCORE_GFX_CONVERT_FCC_LIMITED_TO_RGB;

    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
      return full_range ? SCORE_GFX_CONVERT_BT601_FULL_TO_RGB
                        : SCORE_GFX_CONVERT_BT601_LIMITED_TO_RGB;

    case AVCOL_SPC_SMPTE240M:
      return full_range ? SCORE_GFX_CONVERT_SMPTE240M_FULL_TO_RGB
                        : SCORE_GFX_CONVERT_SMPTE240M_LIMITED_TO_RGB;

    case AVCOL_SPC_YCGCO:
      return full_range ? SCORE_GFX_CONVERT_YCGCO_FULL_TO_RGB
                        : SCORE_GFX_CONVERT_YCGCO_LIMITED_TO_RGB;

    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
      // NOTE: BT.2020 constant luminance requires a different decoding path.
      // In practice CL content is extremely rare; treating as NCL is a
      // reasonable approximation.
    case AVCOL_SPC_SMPTE2085:
    case AVCOL_SPC_CHROMA_DERIVED_NCL:
    case AVCOL_SPC_CHROMA_DERIVED_CL:
    case AVCOL_SPC_ICTCP:
      return bt2020shader(d);

    default:
    case AVCOL_SPC_NB:
    case AVCOL_SPC_UNSPECIFIED:
    case AVCOL_SPC_RESERVED:
      break;
  }

  // Fallback based on resolution; assume limited range if unspecified
  if(d.width >= 1280)
    return full_range ? SCORE_GFX_CONVERT_BT709_FULL_TO_RGB
                      : SCORE_GFX_CONVERT_BT709_LIMITED_TO_RGB;
  else
    return full_range ? SCORE_GFX_CONVERT_BT601_FULL_TO_RGB
                      : SCORE_GFX_CONVERT_BT601_LIMITED_TO_RGB;
}
}

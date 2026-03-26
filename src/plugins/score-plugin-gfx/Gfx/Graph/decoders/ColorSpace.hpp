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
//  Computed from first principles: BT.2020->XYZ->BT.709, D65 white.
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
//  Wide-gamut HDR pipeline building blocks
//
//  These are reusable GLSL fragments for building the various
//  BT.2020 output modes (SDR, Passthrough, Linear, Normalized).
//  Also supports ICtCp (H.273 MatrixCoefficients=14),
//  SMPTE 2085 Y'D'zD'x (MatrixCoefficients=11),
//  and Display P3 input primaries.
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

// --- HLG OOTF (scene-linear -> display-linear) ---
//
//  BT.2100 Section 8.3.3: the OOTF maps scene-referred light
//  to display-referred light. For HLG, this is essential because
//  the inverse OETF only gives scene-linear — the OOTF provides
//  the display rendering intent adapted to the target luminance.
//
//  Fd = Lw * Ys^(gamma-1) * E
//  gamma = 1.2 + 0.42 * log10(Lw / 1000)
//
//  Output is in nits (absolute luminance).
//  For Lw=1000: gamma=1.2, mild highlight boost
//  For Lw=100:  gamma=0.78, dynamic range compression for SDR
//  For Lw=200:  gamma=0.91, slight compression

static constexpr auto BT2020_HLG_OOTF = R"_(
vec3 applyHlgOotf(vec3 scene, float Lw) {
  const vec3 hlgLuma = vec3(0.2627, 0.6780, 0.0593);
  float gamma = 1.2 + 0.42 * log(Lw / 1000.0) / log(10.0);
  float Ys = dot(hlgLuma, scene);
  // Guard against Ys=0 (black) which would cause pow(0, negative) = inf
  if (Ys <= 0.0) return vec3(0.0);
  return Lw * pow(Ys, gamma - 1.0) * scene;
}
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
//  ICtCp decoding (H.273 MatrixCoefficients = 14)
//
//  ICtCp is an alternative colour difference encoding defined
//  in ITU-R BT.2100. Unlike BT.2020 NCL Y'Cb'Cr', the three
//  channels are encoded in a perceptual space:
//    1. Linear BT.2020 RGB -> linear LMS (crosstalk matrix)
//    2. Linear LMS -> PQ-encoded LMS (or HLG-encoded)
//    3. PQ-encoded LMS -> ICtCp (encoding matrix)
//
//  Decoding reverses this:
//    1. ICtCp -> PQ-encoded LMS (inverse of encoding matrix)
//    2. PQ-encoded LMS -> linear LMS (PQ EOTF)
//    3. Linear LMS -> linear BT.2020 RGB (inverse of crosstalk)
//
//  Two encoding matrix variants exist (H.273 Table 4, value 14):
//    - PQ: H.273 equations 79–81
//    - HLG: H.273 equations 82–84
//
//  References:
//    - ITU-R BT.2100-2, Section 8
//    - ITU-T H.273, Section 8.3, MatrixCoefficients = 14
// ============================================================

// ICtCp -> PQ-encoded LMS (inverse of H.273 eqs 79-81)
static constexpr auto ICTCP_PQ_TO_LMS = R"_(
const mat3 ictcpToLms = mat3(
   1.000000000000000,  1.000000000000000,  1.000000000000000,
   0.008609037037933, -0.008609037037933,  0.560031335710679,
   0.111029625003026, -0.111029625003026, -0.320627174987319
);
)_";

// ICtCp -> HLG-encoded LMS (inverse of H.273 eqs 82-84)
static constexpr auto ICTCP_HLG_TO_LMS = R"_(
const mat3 ictcpToLms = mat3(
   1.000000000000000,  1.000000000000000,  1.000000000000000,
   0.015718580108730, -0.015718580108730,  1.021271079842234,
   0.209581068116406, -0.209581068116406, -0.605274490992431
);
)_";

// Linear LMS -> linear BT.2020 RGB (inverse of H.273 eqs 14-16)
static constexpr auto LMS_TO_BT2020_RGB = R"_(
const mat3 lmsToBt2020 = mat3(
   3.436606694333078, -0.791329555598929, -0.025949899690593,
  -2.506452118656270,  1.983600451792291, -0.098913714711726,
   0.069845424323191, -0.192270896193362,  1.124863614402319
);
)_";

// ============================================================
//  SMPTE 2085 Y'D'zD'x decoding (H.273 MatrixCoefficients = 11)
//
//  Y'D'zD'x is used in some Dolby Cinema content. The encoding
//  (H.273 equations 76-78) is:
//    E'_Y  = E'_G
//    E'_PB = (0.986566 * E'_B − E'_Y) / 2.0   (D'z)
//    E'_PR = (0.991902 * E'_R − E'_Y) / 2.0   (D'x)
//
//  The inverse (D'zD'x -> R'G'B') is pre-computed below.
//  Note: the OETF-encoded R', G', B' values share the same
//  transfer characteristic as BT.2020, so the same EOTF
//  pipeline applies after decoding.
//
//  Reference: SMPTE ST 2085, ITU-T H.273 Section 8.3 value 11
// ============================================================

static constexpr auto SMPTE2085_YUV_MATRIX_LIMITED = R"_(
// SMPTE 2085: Y'D'zD'x -> R'G'B' inverse matrix
// Range scaling (255/219 for Y, 255/224 for D'z/D'x) baked in.
const mat3 uYuvToRgbColorTransform = mat3(
   1.173889720601265,  1.164383561643836,  1.180238890904243,
   0.000000000000000,  0.000000000000000,  2.307788545607404,
   2.295373650104259,  0.000000000000000,  0.000000000000000
);
const vec3 yuvOffset = vec3(0.0625, 0.5, 0.5);
)_";

static constexpr auto SMPTE2085_YUV_MATRIX_FULL = R"_(
const mat3 uYuvToRgbColorTransform = mat3(
   1.008164112986969,  1.000000000000000,  1.013616929835409,
   0.000000000000000,  0.000000000000000,  2.027233859670817,
   2.016328225973937,  0.000000000000000,  0.000000000000000
);
const vec3 yuvOffset = vec3(0.0, 0.5, 0.5);
)_";

// ============================================================
//  Display P3 gamut conversion matrices
//
//  Display P3 (H.273 ColourPrimaries = 12, SMPTE EG 432-1)
//  uses the same primaries as DCI-P3 but with D65 white point.
//  These matrices convert between P3 and BT.2020/BT.709.
//
//  Computed from CIE 1931 chromaticity coordinates via XYZ:
//    P3:     R(0.680,0.320) G(0.265,0.690) B(0.150,0.060) W=D65
//    BT.709: R(0.640,0.330) G(0.300,0.600) B(0.150,0.060) W=D65
//    BT.2020:R(0.708,0.292) G(0.170,0.797) B(0.131,0.046) W=D65
// ============================================================

static constexpr auto DISPLAY_P3_TO_BT2020_GAMUT = R"_(
const mat3 gamutConvert = mat3(
   0.753833034361722,  0.045743848965358, -0.001210340354518,
   0.198597369052617,  0.941777219811693,  0.017601717301090,
   0.047569596585662,  0.012478931222948,  0.983608623053428
);
)_";

static constexpr auto DISPLAY_P3_TO_BT709_GAMUT = R"_(
const mat3 gamutConvert = mat3(
   1.224940176280561, -0.042056954709688, -0.019637554590334,
  -0.224940176280560,  1.042056954709688, -0.078636045550632,
   0.000000000000000,  0.000000000000000,  1.098273600140966
);
)_";

// ============================================================
//  Shader generators for each OutputFormat
// ============================================================

// Helper: resolve Auto tonemap to the best choice for this content
static inline ::Video::Tonemap resolvedTonemap(const Video::ImageFormat& d)
{
  if(d.tonemap == ::Video::Tonemap::Auto)
    return resolveAutoTonemap(static_cast<int>(d.color_trc));
  return d.tonemap;
}

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
//
// For HLG: after inverse OETF the signal is scene-linear [0, ~1.0].
// The Linear/Normalized modes output scene-linear directly (OOTF is
// deferred to the ISF shader pipeline where the user can configure
// the display peak). Factor = 1.0.
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
    // HLG inverse OETF: ~1.0 = scene peak.
    // Scene-linear output for Linear/Normalized modes.
    return 1.0f;
  }
  else
  {
    return 1.0f;
  }
}

// Helper: target display luminance for HLG OOTF in SDR mode.
// BT.2408 reference white = 203 nits. For SDR displays, typical
// peak luminance is 100-400 nits. We use sdrPeakNits as target.
static inline float bt2020_hlgDisplayPeakNits(const Video::ImageFormat& d)
{
  // Use sdr peak as the display target. The OOTF's system gamma
  // will naturally compress the dynamic range for this luminance.
  return 203.0f;
}

// ──────────────────────────────────────────────────────────────
//  OutputFormat::Passthrough
//  YUV->RGB only. No EOTF. PQ values stay PQ-encoded, BT.2020 primaries.
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
//  YUV->RGB -> EOTF -> linear BT.2020.
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
//  YUV->RGB -> EOTF -> divide by peak -> linear BT.2020, 1.0 = content peak.
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
//  Full HDR->SDR pipeline, uses the selected Tonemap algorithm.
//
//  For PQ content:
//    YUV->RGB -> PQ EOTF -> normalize(1.0=peak) -> tonemap -> gamut -> sRGB OETF
//
//  For HLG content:
//    YUV->RGB -> HLG inv.OETF -> OOTF(Lw) -> normalize(1.0=peak) -> tonemap -> gamut -> sRGB OETF
//    The OOTF converts scene-linear to display-linear, naturally
//    adapting the rendering to the target display luminance.
//    Without it, the tonemapper receives scene-referred values
//    with the wrong peak assumption.
//
//  The gamut conversion order depends on the tonemapper type:
//
//  Luminance-based tonemappers (BT.2390, BT.2446, Reinhard):
//    Gamut-agnostic, so we tonemap in BT.2020 then convert:
//      … -> tonemap(BT.2020) -> gamut -> sRGB OETF
//
//  Per-channel tonemappers (ACES, AgX, Hable, PBR Neutral):
//    Assume BT.709 input, we must convert gamut first:
//      … -> gamut -> tonemap(BT.709) -> sRGB OETF
// ──────────────────────────────────────────────────────────────

static inline QString bt2020shader_sdr(const Video::ImageFormat& d)
{
  QString shader;
  shader.reserve(8192);

  const bool isHLG = (d.color_trc == AVCOL_TRC_ARIB_STD_B67);

  // 1. YUV to RGB matrix
  bt2020_appendYuvMatrix(shader, d);

  // 2. EOTF (inverse OETF for HLG, PQ EOTF for PQ)
  bt2020_appendEotf(shader, d);

  // 3. HLG OOTF (only for HLG content)
  if(isHLG)
    shader += BT2020_HLG_OOTF;

  // 4. Gamut conversion matrix (BT.2020 -> BT.709)
  shader += BT2020_TO_BT709_GAMUT;

  // 5. sRGB OETF
  shader += SRGB_OETF;

  // 6. Normalization and peak luminance
  //
  // For PQ: EOTF output is 1.0 = 10000 nits. Normalize to 1.0 = content peak.
  // For HLG: After OOTF, output is in nits. Normalize by content peak.
  //   Content peak for HLG = display peak Lw (the OOTF scales to [0, Lw] nits).
  float contentPeak;
  float normFactor;
  if(isHLG)
  {
    // After OOTF(Lw), the peak is Lw nits.
    const float hlgDisplayPeak = bt2020_hlgDisplayPeakNits(d);
    contentPeak = hlgDisplayPeak;
    // Normalize: divide by contentPeak to get 1.0 = peak
    normFactor = 1.0f / contentPeak;
    shader += QString("const float hlgDisplayLw = %1;\n").arg(hlgDisplayPeak, 0, 'f', 1);
  }
  else
  {
    contentPeak = bt2020_contentPeakNits(d);
    normFactor = bt2020_eotfToNormalizedFactor(d);
  }
  shader += QString("const float eotfNormFactor = %1;\n").arg(normFactor, 0, 'f', 6);

  // 7. Tone mapping function
  const float sdrPeak = 203.0f; // BT.2408 reference white
  const auto effectiveTonemap = resolvedTonemap(d);
  shader += tonemapShader(effectiveTonemap, contentPeak, sdrPeak);

  // 8. convert_to_rgb: the complete pipeline
  const bool lumBased = isLuminanceBasedTonemap(effectiveTonemap);

  if(isHLG)
  {
    // HLG-specific pipeline with OOTF
    if(lumBased)
    {
      shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  vec3 rgb = clamp(uYuvToRgbColorTransform * (tex.xyz - yuvOffset), 0.0, 1.0);
  vec3 sceneLinear = applyEotf(rgb);

  // OOTF: scene-linear -> display-linear (nits)
  vec3 displayLinear = applyHlgOotf(sceneLinear, hlgDisplayLw);

  // Normalize: 1.0 = content peak
  displayLinear *= eotfNormFactor;

  vec3 tonemapped = tonemap(displayLinear);
  vec3 linearBt709 = clamp(gamutConvert * tonemapped, 0.0, 1.0);
  return vec4(srgbOetf(linearBt709), 1.0);
}
)_";
    }
    else
    {
      shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  vec3 rgb = clamp(uYuvToRgbColorTransform * (tex.xyz - yuvOffset), 0.0, 1.0);
  vec3 sceneLinear = applyEotf(rgb);
  vec3 displayLinear = applyHlgOotf(sceneLinear, hlgDisplayLw);
  displayLinear *= eotfNormFactor;

  vec3 linearBt709 = gamutConvert * displayLinear;
  vec3 tonemapped = tonemap(linearBt709);
  return vec4(srgbOetf(clamp(tonemapped, 0.0, 1.0)), 1.0);
}
)_";
    }
  }
  else
  {
    // PQ and other transfer functions (original pipeline)
    if(lumBased)
    {
      shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  vec3 rgb = clamp(uYuvToRgbColorTransform * (tex.xyz - yuvOffset), 0.0, 1.0);
  vec3 linearBt2020 = applyEotf(rgb);
  linearBt2020 *= eotfNormFactor;

  vec3 tonemapped = tonemap(linearBt2020);
  vec3 linearBt709 = clamp(gamutConvert * tonemapped, 0.0, 1.0);
  return vec4(srgbOetf(linearBt709), 1.0);
}
)_";
    }
    else
    {
      shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  vec3 rgb = clamp(uYuvToRgbColorTransform * (tex.xyz - yuvOffset), 0.0, 1.0);
  vec3 linearBt2020 = applyEotf(rgb);
  linearBt2020 *= eotfNormFactor;

  vec3 linearBt709 = gamutConvert * linearBt2020;
  vec3 tonemapped = tonemap(linearBt709);
  return vec4(srgbOetf(clamp(tonemapped, 0.0, 1.0)), 1.0);
}
)_";
    }
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
//  ICtCp shader generators (H.273 MatrixCoefficients = 14)
//
//  The decoding path is fundamentally different from BT.2020 NCL:
//    ICtCp -> PQ/HLG-encoded LMS -> linear LMS -> linear BT.2020 RGB
//
//  After this, the data is in linear BT.2020 primaries and can
//  be fed into the same output pipelines (SDR tonemap, Linear,
//  Normalized, Passthrough).
// ============================================================

// Helper: emit ICtCp->LMS inverse matrix based on transfer
static inline void ictcp_appendInverseMatrix(QString& shader, const Video::ImageFormat& d)
{
  if(d.color_trc == AVCOL_TRC_ARIB_STD_B67)
    shader += ICTCP_HLG_TO_LMS;
  else
    shader += ICTCP_PQ_TO_LMS; // Default to PQ (most common for ICtCp)
}

// ICtCp Linear: decode to linear BT.2020 RGB
static inline QString ictcpshader_linear(const Video::ImageFormat& d)
{
  QString shader;
  shader.reserve(4096);

  ictcp_appendInverseMatrix(shader, d);
  shader += LMS_TO_BT2020_RGB;
  bt2020_appendEotf(shader, d);

  // ICtCp channels are stored with the same range encoding as YCbCr:
  // limited range: I in [16/255,235/255], Ct/Cp in [16/255,240/255]
  // full range: I in [0,1], Ct/Cp centered at 0.5
  if(d.color_range == AVCOL_RANGE_MPEG)
  {
    shader += R"_(
const float yScale = 255.0 / 219.0;
const float yOffset = 16.0 / 255.0;
const float cScale = 255.0 / 224.0;
const float cOffset = 128.0 / 255.0;
vec4 convert_to_rgb(vec4 tex) {
  // Step 1: Unpack limited range ICtCp
  float I  = (tex.x - yOffset) * yScale;
  float Ct = (tex.y - cOffset) * cScale;
  float Cp = (tex.z - cOffset) * cScale;

  // Step 2: ICtCp -> transfer-encoded LMS
  vec3 lmsPQ = ictcpToLms * vec3(I, Ct, Cp);

  // Step 3: EOTF -> linear LMS
  vec3 lmsLinear = applyEotf(clamp(lmsPQ, 0.0, 1.0));

  // Step 4: Linear LMS -> linear BT.2020 RGB
  return vec4(lmsToBt2020 * lmsLinear, 1.0);
}
)_";
  }
  else
  {
    shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  float I  = tex.x;
  float Ct = tex.y - 0.5;
  float Cp = tex.z - 0.5;

  vec3 lmsPQ = ictcpToLms * vec3(I, Ct, Cp);
  vec3 lmsLinear = applyEotf(clamp(lmsPQ, 0.0, 1.0));
  return vec4(lmsToBt2020 * lmsLinear, 1.0);
}
)_";
  }

  return shader;
}

// ICtCp Normalized: linear BT.2020, 1.0 = content peak
static inline QString ictcpshader_normalized(const Video::ImageFormat& d)
{
  QString shader;
  shader.reserve(4096);

  ictcp_appendInverseMatrix(shader, d);
  shader += LMS_TO_BT2020_RGB;
  bt2020_appendEotf(shader, d);

  const float normFactor = bt2020_eotfToNormalizedFactor(d);
  shader += QString("const float eotfNormFactor = %1;\n").arg(normFactor, 0, 'f', 4);

  if(d.color_range == AVCOL_RANGE_MPEG)
  {
    shader += R"_(
const float yScale = 255.0 / 219.0;
const float yOffset = 16.0 / 255.0;
const float cScale = 255.0 / 224.0;
const float cOffset = 128.0 / 255.0;
vec4 convert_to_rgb(vec4 tex) {
  float I  = (tex.x - yOffset) * yScale;
  float Ct = (tex.y - cOffset) * cScale;
  float Cp = (tex.z - cOffset) * cScale;
  vec3 lmsPQ = ictcpToLms * vec3(I, Ct, Cp);
  vec3 lmsLinear = applyEotf(clamp(lmsPQ, 0.0, 1.0));
  vec3 bt2020 = lmsToBt2020 * lmsLinear;
  return vec4(bt2020 * eotfNormFactor, 1.0);
}
)_";
  }
  else
  {
    shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  float I  = tex.x;
  float Ct = tex.y - 0.5;
  float Cp = tex.z - 0.5;
  vec3 lmsPQ = ictcpToLms * vec3(I, Ct, Cp);
  vec3 lmsLinear = applyEotf(clamp(lmsPQ, 0.0, 1.0));
  vec3 bt2020 = lmsToBt2020 * lmsLinear;
  return vec4(bt2020 * eotfNormFactor, 1.0);
}
)_";
  }

  return shader;
}

// ICtCp SDR: full tonemap pipeline, output = sRGB
static inline QString ictcpshader_sdr(const Video::ImageFormat& d)
{
  QString shader;
  shader.reserve(8192);

  ictcp_appendInverseMatrix(shader, d);
  shader += LMS_TO_BT2020_RGB;
  bt2020_appendEotf(shader, d);
  shader += BT2020_TO_BT709_GAMUT;
  shader += SRGB_OETF;

  const float normFactor = bt2020_eotfToNormalizedFactor(d);
  shader += QString("const float eotfNormFactor = %1;\n").arg(normFactor, 0, 'f', 4);

  const float contentPeak = bt2020_contentPeakNits(d);
  const float sdrPeak = 203.0f;
  const auto effectiveTonemap = resolvedTonemap(d);
  shader += tonemapShader(effectiveTonemap, contentPeak, sdrPeak);

  const bool lumBased = isLuminanceBasedTonemap(effectiveTonemap);
  const char* rangeUnpack;
  if(d.color_range == AVCOL_RANGE_MPEG)
  {
    shader += R"_(
const float yScale = 255.0 / 219.0;
const float yOffset = 16.0 / 255.0;
const float cScale = 255.0 / 224.0;
const float cOffset = 128.0 / 255.0;
)_";
    rangeUnpack = R"_(
  float I  = (tex.x - yOffset) * yScale;
  float Ct = (tex.y - cOffset) * cScale;
  float Cp = (tex.z - cOffset) * cScale;
)_";
  }
  else
  {
    rangeUnpack = R"_(
  float I  = tex.x;
  float Ct = tex.y - 0.5;
  float Cp = tex.z - 0.5;
)_";
  }

  if(lumBased)
  {
    shader += QString(R"_(
vec4 convert_to_rgb(vec4 tex) {
  %1
  vec3 lmsPQ = ictcpToLms * vec3(I, Ct, Cp);
  vec3 lmsLinear = applyEotf(clamp(lmsPQ, 0.0, 1.0));
  vec3 linearBt2020 = lmsToBt2020 * lmsLinear;
  linearBt2020 *= eotfNormFactor;
  vec3 tonemapped = tonemap(linearBt2020);
  vec3 linearBt709 = clamp(gamutConvert * tonemapped, 0.0, 1.0);
  return vec4(srgbOetf(linearBt709), 1.0);
}
)_").arg(rangeUnpack);
  }
  else
  {
    shader += QString(R"_(
vec4 convert_to_rgb(vec4 tex) {
  %1
  vec3 lmsPQ = ictcpToLms * vec3(I, Ct, Cp);
  vec3 lmsLinear = applyEotf(clamp(lmsPQ, 0.0, 1.0));
  vec3 linearBt2020 = lmsToBt2020 * lmsLinear;
  linearBt2020 *= eotfNormFactor;
  vec3 linearBt709 = gamutConvert * linearBt2020;
  vec3 tonemapped = tonemap(linearBt709);
  return vec4(srgbOetf(clamp(tonemapped, 0.0, 1.0)), 1.0);
}
)_").arg(rangeUnpack);
  }

  return shader;
}

// ICtCp Passthrough: just decode ICtCp to RGB, no EOTF
static inline QString ictcpshader_passthrough(const Video::ImageFormat& d)
{
  QString shader;
  shader.reserve(2048);

  ictcp_appendInverseMatrix(shader, d);

  if(d.color_range == AVCOL_RANGE_MPEG)
  {
    shader += R"_(
const float yScale = 255.0 / 219.0;
const float yOffset = 16.0 / 255.0;
const float cScale = 255.0 / 224.0;
const float cOffset = 128.0 / 255.0;
vec4 convert_to_rgb(vec4 tex) {
  float I  = (tex.x - yOffset) * yScale;
  float Ct = (tex.y - cOffset) * cScale;
  float Cp = (tex.z - cOffset) * cScale;
  // Decode to PQ-encoded LMS, output as-is (no EOTF, no LMS->RGB)
  vec3 lmsPQ = clamp(ictcpToLms * vec3(I, Ct, Cp), 0.0, 1.0);
  return vec4(lmsPQ, 1.0);
}
)_";
  }
  else
  {
    shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  float I  = tex.x;
  float Ct = tex.y - 0.5;
  float Cp = tex.z - 0.5;
  vec3 lmsPQ = clamp(ictcpToLms * vec3(I, Ct, Cp), 0.0, 1.0);
  return vec4(lmsPQ, 1.0);
}
)_";
  }

  return shader;
}

static inline QString ictcpshader(const Video::ImageFormat& d)
{
  switch(d.output_format)
  {
    case Video::OutputFormat::Passthrough:
      return ictcpshader_passthrough(d);
    case Video::OutputFormat::Linear:
      return ictcpshader_linear(d);
    case Video::OutputFormat::Normalized:
      return ictcpshader_normalized(d);
    case Video::OutputFormat::SDR:
    default:
      return ictcpshader_sdr(d);
  }
}

// ============================================================
//  SMPTE 2085 Y'D'zD'x shader (H.273 MatrixCoefficients = 11)
//
//  Uses the SMPTE 2085 inverse matrix instead of BT.2020 NCL,
//  but the rest of the pipeline (EOTF, gamut, tonemap) is the
//  same since the result is in BT.2020 primaries.
// ============================================================

static inline void smpte2085_appendYuvMatrix(QString& shader, const Video::ImageFormat& d)
{
  if(d.color_range == AVCOL_RANGE_MPEG)
    shader += SMPTE2085_YUV_MATRIX_LIMITED;
  else
    shader += SMPTE2085_YUV_MATRIX_FULL;
}

// SMPTE 2085 uses the same output pipeline as BT.2020 after YUV decode,
// just with a different YUV matrix. Reuse the BT.2020 pipeline structure.
static inline QString smpte2085shader(const Video::ImageFormat& d)
{
  // Build the same pipeline as bt2020shader, but with SMPTE 2085 YUV matrix.
  // For simplicity, replicate the structure for each output mode.
  switch(d.output_format)
  {
    case Video::OutputFormat::Passthrough:
    {
      QString shader;
      shader.reserve(1024);
      smpte2085_appendYuvMatrix(shader, d);
      shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  vec3 rgb = clamp(uYuvToRgbColorTransform * (tex.xyz - yuvOffset), 0.0, 1.0);
  return vec4(rgb, 1.0);
}
)_";
      return shader;
    }
    case Video::OutputFormat::Linear:
    {
      QString shader;
      shader.reserve(2048);
      smpte2085_appendYuvMatrix(shader, d);
      bt2020_appendEotf(shader, d);
      shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  vec3 rgb = clamp(uYuvToRgbColorTransform * (tex.xyz - yuvOffset), 0.0, 1.0);
  return vec4(applyEotf(rgb), 1.0);
}
)_";
      return shader;
    }
    case Video::OutputFormat::Normalized:
    {
      QString shader;
      shader.reserve(2048);
      smpte2085_appendYuvMatrix(shader, d);
      bt2020_appendEotf(shader, d);
      const float normFactor = bt2020_eotfToNormalizedFactor(d);
      shader += QString("const float eotfNormFactor = %1;\n").arg(normFactor, 0, 'f', 4);
      shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  vec3 rgb = clamp(uYuvToRgbColorTransform * (tex.xyz - yuvOffset), 0.0, 1.0);
  vec3 linear = applyEotf(rgb);
  return vec4(linear * eotfNormFactor, 1.0);
}
)_";
      return shader;
    }
    case Video::OutputFormat::SDR:
    default:
    {
      // Reuse the full SDR pipeline — only the YUV matrix differs.
      // Temporarily swap YUV matrix, then delegate to bt2020shader_sdr structure.
      QString shader;
      shader.reserve(8192);
      smpte2085_appendYuvMatrix(shader, d);
      bt2020_appendEotf(shader, d);
      shader += BT2020_TO_BT709_GAMUT;
      shader += SRGB_OETF;

      const float normFactor = bt2020_eotfToNormalizedFactor(d);
      shader += QString("const float eotfNormFactor = %1;\n").arg(normFactor, 0, 'f', 4);
      const float contentPeak = bt2020_contentPeakNits(d);
      const auto effectiveTonemap = resolvedTonemap(d);
      shader += tonemapShader(effectiveTonemap, contentPeak, 203.0f);

      const bool lumBased = isLuminanceBasedTonemap(effectiveTonemap);
      if(lumBased)
      {
        shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  vec3 rgb = clamp(uYuvToRgbColorTransform * (tex.xyz - yuvOffset), 0.0, 1.0);
  vec3 linearBt2020 = applyEotf(rgb) * eotfNormFactor;
  vec3 tonemapped = tonemap(linearBt2020);
  vec3 linearBt709 = clamp(gamutConvert * tonemapped, 0.0, 1.0);
  return vec4(srgbOetf(linearBt709), 1.0);
}
)_";
      }
      else
      {
        shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  vec3 rgb = clamp(uYuvToRgbColorTransform * (tex.xyz - yuvOffset), 0.0, 1.0);
  vec3 linearBt2020 = applyEotf(rgb) * eotfNormFactor;
  vec3 linearBt709 = gamutConvert * linearBt2020;
  vec3 tonemapped = tonemap(linearBt709);
  return vec4(srgbOetf(clamp(tonemapped, 0.0, 1.0)), 1.0);
}
)_";
      }
      return shader;
    }
  }
}

// ============================================================
//  Display P3 input shader (H.273 ColourPrimaries = 11 or 12)
//
//  For content encoded with BT.709 matrix coefficients but
//  Display P3 primaries. Converts P3 -> BT.2020 for the wide-
//  gamut pipeline, or P3 -> BT.709 for SDR output.
//  The YUV->RGB step uses the BT.709 matrix (since that's what
//  the content's MatrixCoefficients says), then a gamut
//  conversion handles the primaries mismatch.
// ============================================================

static inline QString displayP3shader(const Video::ImageFormat& d)
{
  const bool full_range = (d.color_range == AVCOL_RANGE_JPEG);

  switch(d.output_format)
  {
    case Video::OutputFormat::Linear:
    case Video::OutputFormat::Normalized:
    {
      // Output in BT.2020 primaries (wide-gamut preserving) for downstream shaders
      QString shader;
      shader.reserve(2048);

      // Use BT.709 YUV matrix (content is P3 but encoded with 709 coefficients)
      shader += full_range ? "const mat4 conversion_matrix = " SCORE_GFX_BT709_FULL_MATRIX ";\n"
                           : "const mat4 conversion_matrix = " SCORE_GFX_BT709_LIMITED_MATRIX ";\n";

      shader += DISPLAY_P3_TO_BT2020_GAMUT;

      shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  vec3 p3 = (conversion_matrix * tex).rgb;
  // Convert P3 linear -> BT.2020 linear (preserves full gamut)
  return vec4(gamutConvert * p3, 1.0);
}
)_";
      return shader;
    }
    case Video::OutputFormat::SDR:
    default:
    {
      // Convert P3 -> BT.709 for SDR display
      QString shader;
      shader.reserve(2048);

      shader += full_range ? "const mat4 conversion_matrix = " SCORE_GFX_BT709_FULL_MATRIX ";\n"
                           : "const mat4 conversion_matrix = " SCORE_GFX_BT709_LIMITED_MATRIX ";\n";

      shader += DISPLAY_P3_TO_BT709_GAMUT;

      shader += R"_(
vec4 convert_to_rgb(vec4 tex) {
  vec3 p3 = (conversion_matrix * tex).rgb;
  // Convert P3 -> BT.709 (may clip out-of-gamut colors)
  return vec4(clamp(gamutConvert * p3, 0.0, 1.0), 1.0);
}
)_";
      return shader;
    }
    case Video::OutputFormat::Passthrough:
    {
      // Just decode YUV, keep P3 primaries
      return full_range ? SCORE_GFX_CONVERT_BT709_FULL_TO_RGB
                        : SCORE_GFX_CONVERT_BT709_LIMITED_TO_RGB;
    }
  }
}

// ============================================================
//  Chroma-derived NCL matrix coefficients lookup
//  (H.273 MatrixCoefficients = 12)
//
//  Per H.273 equations 39-44, Kr and Kb are derived from the
//  ColourPrimaries chromaticity coordinates. We look up the
//  actual primaries and route to the correct YUV matrix.
//  For BT.2020 primaries this equals BT.2020 NCL (Kr=0.2627).
//  For BT.709 primaries this equals BT.709 (Kr=0.2126).
// ============================================================

static inline QString chromaDerivedNclMatrix(const Video::ImageFormat& d)
{
  const bool full_range = (d.color_range == AVCOL_RANGE_JPEG);

  // Route based on actual color primaries
  switch(d.color_primaries)
  {
    case AVCOL_PRI_BT2020:
      // Kr=0.2627, Kb=0.0593 -> BT.2020 NCL
      return bt2020shader(d);

    case AVCOL_PRI_SMPTE432: // Display P3 (D65)
    case AVCOL_PRI_SMPTE431: // DCI-P3
      // P3 primaries -> Kr≈0.2290, Kb≈0.0792
      // Close enough to BT.709 coefficients for YUV decoding,
      // but gamut is wider. Route through P3 pipeline.
      return displayP3shader(d);

    case AVCOL_PRI_BT709:
    case AVCOL_PRI_UNSPECIFIED:
    default:
      // Kr=0.2126, Kb=0.0722 -> BT.709
      return full_range ? SCORE_GFX_CONVERT_BT709_FULL_TO_RGB
                        : SCORE_GFX_CONVERT_BT709_LIMITED_TO_RGB;

    case AVCOL_PRI_BT470BG:
    case AVCOL_PRI_SMPTE170M:
      // Kr=0.299, Kb=0.114 -> BT.601
      return full_range ? SCORE_GFX_CONVERT_BT601_FULL_TO_RGB
                        : SCORE_GFX_CONVERT_BT601_LIMITED_TO_RGB;

    case AVCOL_PRI_SMPTE240M:
      return full_range ? SCORE_GFX_CONVERT_SMPTE240M_FULL_TO_RGB
                        : SCORE_GFX_CONVERT_SMPTE240M_LIMITED_TO_RGB;
  }
}

// ============================================================
//  Main color matrix selection
// ============================================================

static inline QString colorMatrix(const Video::ImageFormat& d)
{
  const bool full_range = (d.color_range == AVCOL_RANGE_JPEG);

  switch(d.color_space)
  {
    case AVCOL_SPC_RGB:
      return "vec4 convert_to_rgb(vec4 tex) { return tex; }";

    case AVCOL_SPC_BT709:
      // Check if primaries indicate wider gamut than BT.709
      if(d.color_primaries == AVCOL_PRI_SMPTE432
         || d.color_primaries == AVCOL_PRI_SMPTE431)
        return displayP3shader(d);
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
      // NOTE: BT.2020 constant luminance (CL) requires a different
      // decoding path. In practice CL content is extremely rare;
      // treating as NCL is a reasonable approximation.
      return bt2020shader(d);

    case AVCOL_SPC_SMPTE2085:
      // SMPTE 2085 Y'D'zD'x — different encoding matrix,
      // same BT.2020 primaries and EOTF pipeline.
      return smpte2085shader(d);

    case AVCOL_SPC_ICTCP:
      // ICtCp (BT.2100) — completely different encoding via LMS.
      // Separate decoding path through PQ/HLG-encoded LMS domain.
      return ictcpshader(d);

    case AVCOL_SPC_CHROMA_DERIVED_NCL:
      // H.273 MatrixCoefficients=12: derive Kr/Kb from ColourPrimaries.
      return chromaDerivedNclMatrix(d);

    case AVCOL_SPC_CHROMA_DERIVED_CL:
      // H.273 MatrixCoefficients=13: chroma-derived constant luminance.
      // Extremely rare. Fall back to NCL derivation as approximation.
      return chromaDerivedNclMatrix(d);

    default:
    case AVCOL_SPC_NB:
    case AVCOL_SPC_UNSPECIFIED:
    case AVCOL_SPC_RESERVED:
      break;
  }

  // Fallback: check if color_primaries indicate wide gamut
  if(d.color_primaries == AVCOL_PRI_BT2020)
    return bt2020shader(d);
  if(d.color_primaries == AVCOL_PRI_SMPTE432
     || d.color_primaries == AVCOL_PRI_SMPTE431)
    return displayP3shader(d);

  // Fallback based on resolution; assume limited range if unspecified
  if(d.width >= 1280)
    return full_range ? SCORE_GFX_CONVERT_BT709_FULL_TO_RGB
                      : SCORE_GFX_CONVERT_BT709_LIMITED_TO_RGB;
  else
    return full_range ? SCORE_GFX_CONVERT_BT601_FULL_TO_RGB
                      : SCORE_GFX_CONVERT_BT601_LIMITED_TO_RGB;
}
}

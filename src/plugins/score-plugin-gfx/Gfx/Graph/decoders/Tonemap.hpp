#pragma once

// ============================================================
//  Tonemap.hpp — GLSL tone mapping functions for HDR→SDR
//
//  Each function provides a `vec3 tonemap(vec3 color)` that maps
//  linear-light RGB to the [0,1] range.
//
//  Input assumptions:
//    - Linear light RGB values.
//    - For video HDR content (PQ source): values are normalized
//      so that 1.0 = contentPeakNits (set via uniform or const).
//      This means diffuse white ≈ 203/contentPeakNits ≈ 0.203
//      for 1000-nit content.
//    - For CG/ISF content: scene-referred linear, typically 1.0 = white.
//
//  Gamut handling:
//    Tonemappers are classified as either "luminance-based" or "per-channel".
//    - Luminance-based (BT.2390, BT.2446a, Reinhard): operate on a single
//      luminance channel and scale all RGB channels equally. These are
//      gamut-agnostic and should be applied BEFORE gamut conversion.
//    - Per-channel (ACES, AgX, Hable, PBR Neutral): apply curves to
//      individual channels or contain internal color-space matrices.
//      ACES and AgX assume BT.709/sRGB input primaries.
//      These should be applied AFTER converting from BT.2020 to BT.709.
//
//  Output: [0,1] linear-light suitable for OETF encoding (sRGB, gamma 2.2, etc.)
//
//  References:
//    - BT.2390:     ITU-R BT.2390 EETF (hermite spline in PQ domain)
//    - BT.2446a:    Simplified log compression inspired by ITU-R BT.2446 Method A
//    - Reinhard:    Erik Reinhard's simple operator (2002)
//    - Hable:       John Hable / Uncharted 2 filmic curve (GDC 2010)
//    - ACES2:       Stephen Hill's RRT+ODT approximation (BakingLab)
//    - AgX:         Minimal algebraic AgX (no LUT), iolite engine impl
//                   (Troy Sobotka's formation)
//    - PBR_Neutral: Khronos PBR Neutral tone mapper (2024)
//    - Clamp:       Simple clamp to [0,1] (no tone mapping)
//
//  License: Public domain / MIT where applicable.
//           BT.2390/BT.2446a based on ITU specifications.
//           PBR_Neutral from Khronos Group (Apache 2.0).
//           AgX from Troy Sobotka / community implementations.
// ============================================================

#include <QString>
#include <Video/VideoEnums.hpp>

namespace score::gfx
{

// ============================================================
//  Tonemapper classification
//
//  Returns true if the tonemapper operates on luminance only
//  (gamut-agnostic), meaning gamut conversion should happen AFTER.
//  Returns false if it contains per-channel curves or internal
//  color-space matrices (gamut conversion should happen BEFORE).
// ============================================================

static inline bool isLuminanceBasedTonemap(::Video::Tonemap mode)
{
  switch(mode)
  {
    case ::Video::Tonemap::BT_2390:
    case ::Video::Tonemap::BT_2446:
    case ::Video::Tonemap::Reinhard:
      return true;
    default:
      return false;
  }
}

// ============================================================
//  BT.2390 EETF
//
//  The ITU-R BT.2390 Electrical-Electrical Transfer Function.
//  Operates in PQ domain: converts linear→PQ, applies a hermite
//  spline roll-off from the knee point to the target peak, then
//  converts back to linear.
//
//  Reference: ITU-R BT.2390-1, Section 5.4.1
//    KS = 1.5 * maxLum - 0.5
//    T(A) = (A - KS) / (1 - KS)
//    P(B) = h00(T)*KS + h10(T)*(1-KS) + h01(T)*maxLum
//    where h00(t) = 2t³-3t²+1, h10(t) = t³-2t²+t, h01(t) = -2t³+3t²
//
//  The spline maps [KS, srcPeak] → [KS, dstPeak] in PQ domain.
//  Slope at KS = 1 (continuity with 1:1 segment), slope at end = 0.
// ============================================================

static constexpr auto TONEMAP_BT2390 = R"_(
// BT.2390 EETF — operates on max-RGB channel, preserves hue ratios.
// Input: linear light, 1.0 = content peak.
// Output: linear light, [0, 1].

// PQ forward: linear [0,1] (where 1.0 = 10000 nits) → PQ [0,1]
float pqForward(float Y) {
  float Ym1 = pow(max(Y, 0.0), 0.1593017578125);
  return pow((0.8359375 + 18.8515625 * Ym1) / (1.0 + 18.6875 * Ym1), 78.84375);
}

// PQ inverse: PQ [0,1] → linear [0,1] (where 1.0 = 10000 nits)
float pqInverse(float N) {
  float Nm = pow(max(N, 0.0), 1.0 / 78.84375);
  return pow(max(Nm - 0.8359375, 0.0) / (18.8515625 - 18.6875 * Nm), 1.0 / 0.1593017578125);
}

// BT.2390 hermite spline
// Maps [KS, srcPeakPQ] → [KS, dstPeakPQ] with slope=1 at KS, slope=0 at end.
float bt2390_eetf(float E, float KS, float srcPeakPQ, float dstPeakPQ) {
  if (E < KS) return E;
  if (srcPeakPQ <= KS) return dstPeakPQ;

  // Normalize to [0,1] in the [KS, srcPeakPQ] range
  float t = (E - KS) / (srcPeakPQ - KS);
  float t2 = t * t;
  float t3 = t2 * t;

  // Hermite basis functions:
  //   h00(t) = 2t³ - 3t² + 1  (value at t=0: 1, at t=1: 0)
  //   h10(t) = t³ - 2t² + t   (tangent at t=0: 1, at t=1: 0)
  //   h01(t) = -2t³ + 3t²     (value at t=0: 0, at t=1: 1)
  //
  // P(t) = h00(t)*KS + h10(t)*(srcPeakPQ-KS) + h01(t)*dstPeakPQ
  //
  // At t=0: P = KS (continuity with linear segment)
  // At t=1: P = dstPeakPQ (reaches target peak)
  // dP/dt at t=0 = (srcPeakPQ-KS) → dP/dE = 1 (slope continuity)
  // dP/dt at t=1 = 0 (smooth roll-off)
  float p = (2.0 * t3 - 3.0 * t2 + 1.0) * KS
          + (t3 - 2.0 * t2 + t) * (srcPeakPQ - KS)
          + (-2.0 * t3 + 3.0 * t2) * dstPeakPQ;

  return p;
}

vec3 tonemap(vec3 color) {
  const float srcPeakNits = contentPeakNits;
  const float dstPeakNits = sdrPeakNits;

  // Convert nits to PQ domain
  float srcPeakPQ = pqForward(srcPeakNits / 10000.0);
  float dstPeakPQ = pqForward(dstPeakNits / 10000.0);

  // Knee start: BT.2390 formula
  float KS = 1.5 * dstPeakPQ - 0.5;

  // Operate on max-RGB to preserve hue ratios
  float maxC = max(color.r, max(color.g, color.b));
  if (maxC <= 0.0) return color;

  // Convert maxC from normalized (1.0=peak) to absolute PQ
  float absLinear = maxC * srcPeakNits / 10000.0;
  float pqVal = pqForward(absLinear);

  // Apply EETF
  float mappedPQ = bt2390_eetf(pqVal, KS, srcPeakPQ, dstPeakPQ);

  // Convert back to linear, normalize to [0,1] relative to target peak
  float mappedLinear = pqInverse(mappedPQ);
  float dstLinear = pqInverse(dstPeakPQ);
  float ratio = mappedLinear / max(dstLinear, 1e-10);

  // Scale all channels by the same ratio
  float scale = ratio / maxC;
  return clamp(color * scale, 0.0, 1.0);
}
)_";

// ============================================================
//  BT.2446-inspired log compression
//
//  Simplified logarithmic tone curve with parametric adaptation.
//  Inspired by BT.2446 Method A's approach of operating in
//  luminance space with chroma scaling.
//
//  Note: NOT a verbatim ITU-R BT.2446 Method A implementation.
//  The actual spec uses a more complex piecewise function.
// ============================================================

static constexpr auto TONEMAP_BT2446A = R"_(
// BT.2446-inspired log tone curve with chroma scaling
// Input: linear light, 1.0 = content peak.
// Output: linear light [0,1].

vec3 tonemap(vec3 color) {
  const float srcPeak = contentPeakNits;
  const float dstPeak = sdrPeakNits;

  // BT.2020 luminance coefficients (input is BT.2020 primaries)
  const vec3 lumaCoeff = vec3(0.2627, 0.6780, 0.0593);

  // Convert to absolute nits
  vec3 absColor = color * srcPeak;

  // Compute luminance
  float Y = dot(absColor, lumaCoeff);
  if (Y <= 0.0) return vec3(0.0);

  // Normalized luminance (0-1 range relative to srcPeak)
  float Yn = Y / srcPeak;

  // Parametric log compression: rho adapts to source peak
  float rho = 1.0 + 32.0 * pow(srcPeak / 10000.0, 1.0 / 2.4);
  float Yt = log(1.0 + (rho - 1.0) * Yn) / log(rho);

  // Scale to destination peak
  float Yout = Yt * dstPeak;

  // Chroma scaling: scale proportionally to luminance change
  float chromaScale = Yout / Y;

  vec3 result = absColor * chromaScale;
  result /= dstPeak;

  return clamp(result, 0.0, 1.0);
}
)_";

// ============================================================
//  Reinhard (extended with white point)
//
//  Uses BT.2020 luminance coefficients because this tonemapper
//  is applied before gamut conversion (luminance-based).
//
//  Reference: Reinhard et al. 2002
// ============================================================

static constexpr auto TONEMAP_REINHARD = R"_(
// Reinhard tone mapping (extended with white point)
// Input: linear light in BT.2020 primaries, 1.0 = content peak.
// Output: linear light [0,1].

vec3 tonemap(vec3 color) {
  vec3 c = color * (contentPeakNits / sdrPeakNits);

  // BT.2020 luminance coefficients
  const vec3 lumaCoeff = vec3(0.2627, 0.6780, 0.0593);
  float Lin = dot(c, lumaCoeff);
  if (Lin <= 0.0) return vec3(0.0);

  float whitePoint = contentPeakNits / sdrPeakNits;
  float wp2 = whitePoint * whitePoint;

  // Extended Reinhard
  float Lout = (Lin * (1.0 + Lin / wp2)) / (1.0 + Lin);

  return clamp(c * (Lout / Lin), 0.0, 1.0);
}
)_";

// ============================================================
//  Hable (Uncharted 2)
//
//  Per-channel filmic curve, gamut-agnostic.
//
//  Reference: John Hable, GDC 2010
//  Verified against three.js Uncharted2Helper.
// ============================================================

static constexpr auto TONEMAP_HABLE = R"_(
// Hable / Uncharted 2 filmic tone mapping
// Input: linear light, 1.0 = content peak.
// Output: linear light [0,1].

vec3 hableCurve(vec3 x) {
  const float A = 0.15; // Shoulder strength
  const float B = 0.50; // Linear strength
  const float C = 0.10; // Linear angle
  const float D = 0.20; // Toe strength
  const float E = 0.02; // Toe numerator
  const float F = 0.30; // Toe denominator
  return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 tonemap(vec3 color) {
  vec3 c = color * (contentPeakNits / sdrPeakNits);

  float exposureBias = 2.0;
  vec3 mapped = hableCurve(c * exposureBias);

  float W = contentPeakNits / sdrPeakNits;
  vec3 whiteScale = 1.0 / hableCurve(vec3(W));

  return clamp(mapped * whiteScale, 0.0, 1.0);
}
)_";

// ============================================================
//  ACES (Stephen Hill RRT+ODT approximation)
//
//  IMPORTANT: Input must be in BT.709/sRGB primaries, NOT BT.2020.
//
//  Reference: TheRealMJP/BakingLab/ACES.hlsl
// ============================================================

static constexpr auto TONEMAP_ACES2 = R"_(
// ACES filmic tone mapping (Hill fit)
// Input: linear light in BT.709/sRGB primaries, 1.0 = content peak.
// Output: linear light [0,1] in BT.709/sRGB primaries.

vec3 tonemap(vec3 color) {
  vec3 c = color * (contentPeakNits / sdrPeakNits);

  // sRGB/BT.709 → AP1
  const mat3 ACESInputMat = mat3(
    0.59719, 0.07600, 0.02840,
    0.35458, 0.90834, 0.13383,
    0.04823, 0.01566, 0.83777
  );

  // AP1 → sRGB/BT.709
  const mat3 ACESOutputMat = mat3(
     1.60475, -0.10208, -0.00327,
    -0.53108,  1.10813, -0.07276,
    -0.07367, -0.00605,  1.07602
  );

  vec3 v = ACESInputMat * c;
  vec3 a = v * (v + 0.0245786) - 0.000090537;
  vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
  v = a / b;
  v = ACESOutputMat * v;

  return clamp(v, 0.0, 1.0);
}
)_";

// ============================================================
//  AgX (minimal algebraic, no LUT)
//
//  IMPORTANT: Input must be in BT.709/sRGB primaries, NOT BT.2020.
//
//  Reference: iolite-engine.com/blog_posts/minimal_agx_implementation
//  Matrices verified as exact inverses.
// ============================================================

static constexpr auto TONEMAP_AGX = R"_(
// Minimal AgX tone mapping (algebraic, no LUT)
// Input: linear light in BT.709/sRGB primaries, 1.0 = content peak.
// Output: linear light [0,1] in BT.709/sRGB primaries.

vec3 agxDefaultContrastApprox(vec3 x) {
  vec3 x2 = x * x;
  vec3 x4 = x2 * x2;
  return + 15.5     * x4 * x2
         - 40.14    * x4 * x
         + 31.96    * x4
         - 6.868    * x2 * x
         + 0.4298   * x2
         + 0.1191   * x
         - 0.00232;
}

vec3 agx(vec3 color) {
  // sRGB/BT.709 linear → AgX log space
  const mat3 agxTransform = mat3(
    0.842479062253094,  0.0423282422610123, 0.0423756549057051,
    0.0784335999999992, 0.878468636469772,  0.0784336,
    0.0792237451477643, 0.0791661274605434, 0.879142973793104
  );

  vec3 val = agxTransform * color;
  val = max(val, 1e-10);

  const float minEV = -12.47393;
  const float maxEV = 4.026069;
  val = clamp(log2(val), minEV, maxEV);
  val = (val - minEV) / (maxEV - minEV);

  val = agxDefaultContrastApprox(val);
  return val;
}

vec3 agxEotf(vec3 color) {
  // AgX → sRGB/BT.709 linear
  const mat3 agxInvTransform = mat3(
     1.19687900512017,  -0.0528968517574562, -0.0529716355144438,
    -0.0980208811401368,  1.15190312990417,  -0.0980434501171241,
    -0.0990297440797205, -0.0989611768448433,  1.15107367264116
  );
  return agxInvTransform * color;
}

vec3 tonemap(vec3 color) {
  vec3 c = color * (contentPeakNits / sdrPeakNits);

  vec3 val = agx(c);
  val = agxEotf(val);

  return clamp(val, 0.0, 1.0);
}
)_";

// ============================================================
//  Khronos PBR Neutral
//
//  Gamut-agnostic (operates on per-channel peak/min).
//
//  Reference: https://github.com/KhronosGroup/ToneMapping
//  Verified against reference GLSL.
// ============================================================

static constexpr auto TONEMAP_PBR_NEUTRAL = R"_(
// Khronos PBR Neutral tone mapping (2024)
// Input: linear light, 1.0 = content peak.
// Output: linear light [0,1].

vec3 pbrNeutralToneMapping(vec3 color) {
  const float startCompression = 0.8 - 0.04;
  const float desaturation = 0.15;

  float x = min(color.r, min(color.g, color.b));
  float offset = (x < 0.08) ? x - 6.25 * x * x : 0.04;
  color -= offset;

  float peak = max(color.r, max(color.g, color.b));
  if (peak < startCompression) return color;

  float d = 1.0 - startCompression;
  float newPeak = 1.0 - d * d / (peak + d - startCompression);
  color *= newPeak / peak;

  float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
  return mix(color, vec3(newPeak), g);
}

vec3 tonemap(vec3 color) {
  vec3 c = color * (contentPeakNits / sdrPeakNits);
  return clamp(pbrNeutralToneMapping(c), 0.0, 1.0);
}
)_";

// ============================================================
//  Clamp (no tone mapping)
// ============================================================

static constexpr auto TONEMAP_CLAMP = R"_(
vec3 tonemap(vec3 color) {
  return clamp(color, 0.0, 1.0);
}
)_";

// ============================================================
//  Constants header
// ============================================================

static inline QString tonemapConstants(float contentPeakNits = 1000.0f, float sdrPeakNits = 203.0f)
{
  return QString(R"_(
const float contentPeakNits = %1;
const float sdrPeakNits = %2;
)_")
      .arg(contentPeakNits, 0, 'f', 1)
      .arg(sdrPeakNits, 0, 'f', 1);
}

// ============================================================
//  Main entry point
// ============================================================

static inline QString tonemapShader(
    ::Video::Tonemap mode,
    float contentPeakNits = 1000.0f,
    float sdrPeakNits = 203.0f)
{
  QString shader;
  shader.reserve(4096);

  shader += tonemapConstants(contentPeakNits, sdrPeakNits);

  switch(mode)
  {
    case ::Video::Tonemap::BT_2390:   shader += TONEMAP_BT2390;      break;
    case ::Video::Tonemap::BT_2446:   shader += TONEMAP_BT2446A;     break;
    case ::Video::Tonemap::Reinhard:   shader += TONEMAP_REINHARD;    break;
    case ::Video::Tonemap::Hable:      shader += TONEMAP_HABLE;       break;
    case ::Video::Tonemap::ACES2:      shader += TONEMAP_ACES2;       break;
    case ::Video::Tonemap::AgX:        shader += TONEMAP_AGX;         break;
    case ::Video::Tonemap::PBR_Neutral: shader += TONEMAP_PBR_NEUTRAL; break;
    case ::Video::Tonemap::Clamp:
    default:                            shader += TONEMAP_CLAMP;       break;
  }

  return shader;
}

// ============================================================
//  Utility GLSL fragments
// ============================================================

static constexpr auto TONEMAP_BT2020_TO_BT709_MATRIX = R"_(
const mat3 bt2020ToBt709 = mat3(
   1.6605, -0.1246, -0.0182,
  -0.5876,  1.1329, -0.1006,
  -0.0728, -0.0083,  1.1187
);
)_";

static constexpr auto TONEMAP_BT2020_TO_DISPLAY_P3_MATRIX = R"_(
const mat3 bt2020ToDisplayP3 = mat3(
   1.3434, -0.0653, -0.0029,
  -0.2822,  1.0760, -0.0416,
  -0.0612, -0.0107,  1.0445
);
)_";

static constexpr auto TONEMAP_SRGB_OETF = R"_(
vec3 srgbOetf(vec3 c) {
  vec3 lo = c * 12.92;
  vec3 hi = 1.055 * pow(max(c, 0.0), vec3(1.0 / 2.4)) - 0.055;
  return mix(lo, hi, step(vec3(0.0031308), c));
}
)_";

static constexpr auto TONEMAP_GAMMA22_OETF = R"_(
vec3 gamma22Oetf(vec3 c) {
  return pow(max(c, 0.0), vec3(1.0 / 2.2));
}
)_";

}

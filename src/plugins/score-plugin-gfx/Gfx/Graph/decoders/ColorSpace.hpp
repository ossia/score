#pragma once
#include <Video/VideoInterface.hpp>

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
//  The full HDR path (EOTF, OOTF, OETF) is in bt2020shader().
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
//  BT.2020 HDR pipeline (EOTF / OOTF / OETF)
// ============================================================

static constexpr auto SCORE_GFX_CONVERT_BT2020_TO_RGB_HEADER = R"_(
const int COLOR_TRANSFER_LINEAR = 1;
const int COLOR_TRANSFER_GAMMA_2_2 = 10;
const int COLOR_TRANSFER_ST2084 = 6;
const int COLOR_TRANSFER_HLG = 7;
)_";

static constexpr auto SCORE_GFX_CONVERT_BT2020_TO_RGB_HEADER_LIMITED_RANGE = R"_(
const mat3 uYuvToRgbColorTransform = mat3(
  1.1689f, 1.1689f, 1.1689f,
  0.0000f, -0.1881f, 2.1502f,
  1.6853f, -0.6530f, 0.0000f
);
)_";
static constexpr auto SCORE_GFX_CONVERT_BT2020_TO_RGB_HEADER_FULL_RANGE = R"_(
const mat3 uYuvToRgbColorTransform = mat3(
   1.0000f, 1.0000f, 1.0000f,
   0.0000f, -0.1646f, 1.8814f,
   1.4746f, -0.5714f, 0.0000f
);
)_";
static constexpr auto SCORE_GFX_CONVERT_BT2020_TO_RGB = R"_(

const int uApplyHdrToSdrToneMapping = 1;


// BT.2100 / BT.2020 HLG EOTF for one channel.
float hlgEotfSingleChannel(float hlgChannel) {
  const float a = 0.17883277;
  const float b = 0.28466892;
  const float c = 0.55991073;
  return hlgChannel <= 0.5 ? hlgChannel * hlgChannel / 3.0
                           : (b + exp((hlgChannel - c) / a)) / 12.0;
}

// BT.2100 / BT.2020 HLG EOTF.
vec3 hlgEotf(vec3 hlgColor) {
  return vec3(hlgEotfSingleChannel(hlgColor.r),
              hlgEotfSingleChannel(hlgColor.g),
              hlgEotfSingleChannel(hlgColor.b));
}

// BT.2100 / BT.2020 PQ EOTF.
vec3 pqEotf(vec3 pqColor) {
  const float m1 = (2610.0 / 16384.0);
  const float m2 = (2523.0 / 4096.0) * 128.0;
  const float c1 = (3424.0 / 4096.0);
  const float c2 = (2413.0 / 4096.0) * 32.0;
  const float c3 = (2392.0 / 4096.0) * 32.0;

  vec3 temp = pow(clamp(pqColor, 0.0, 1.0), 1.0 / vec3(m2));
  temp = max(temp - c1, 0.0) / (c2 - c3 * temp);
  return pow(temp, 1.0 / vec3(m1));
}

// Applies the appropriate EOTF to convert nonlinear electrical values to linear
// optical values. Input and output are both normalized to [0, 1].
vec3 applyEotf(vec3 electricalColor) {
  if (uInputColorTransfer == COLOR_TRANSFER_ST2084) {
    return pqEotf(electricalColor);
  } else if (uInputColorTransfer == COLOR_TRANSFER_HLG) {
    return hlgEotf(electricalColor);
  } else {
    // Output red as an obviously visible error.
    return vec3(1.0, 0.0, 0.0);
  }
}

// Apply the HLG BT2020 to BT709 OOTF.
vec3 applyHlgBt2020ToBt709Ootf(vec3 linearRgbBt2020) {
  const mat3 RGB_TO_XYZ_BT2020 =
      mat3(0.63695805f, 0.26270021f, 0.00000000f, 0.14461690f, 0.67799807f,
           0.02807269f, 0.16888098f, 0.05930172f, 1.06098506f);
  const mat3 XYZ_TO_RGB_BT709 =
      mat3(3.24096994f, -0.96924364f, 0.05563008f, -1.53738318f, 1.87596750f,
           -0.20397696f, -0.49861076f, 0.04155506f, 1.05697151f);
  const float hlgGamma = 1.0735674018211279;

  vec3 linearXyzBt2020 = RGB_TO_XYZ_BT2020 * linearRgbBt2020;
  vec3 linearXyzBt709 =
      linearXyzBt2020 * pow(linearXyzBt2020[1], hlgGamma - 1.0);
  vec3 linearRgbBt709 = clamp((XYZ_TO_RGB_BT709 * linearXyzBt709), 0.0, 1.0);
  return linearRgbBt709;
}

// Apply the PQ BT2020 to BT709 OOTF.
vec3 applyPqBt2020ToBt709Ootf(vec3 linearRgbBt2020) {
  float pqPeakLuminance = 10000.0;
  float sdrPeakLuminance = 500.0;

  return linearRgbBt2020 * pqPeakLuminance / sdrPeakLuminance;
}

vec3 applyBt2020ToBt709Ootf(vec3 linearRgbBt2020) {
  if (uInputColorTransfer == COLOR_TRANSFER_ST2084) {
    return applyPqBt2020ToBt709Ootf(linearRgbBt2020);
  } else if (uInputColorTransfer == COLOR_TRANSFER_HLG) {
    return applyHlgBt2020ToBt709Ootf(linearRgbBt2020);
  } else {
    // Output green as an obviously visible error.
    return vec3(0.0, 1.0, 0.0);
  }
}

// BT.2100 / BT.2020 HLG OETF for one channel.
float hlgOetfSingleChannel(float linearChannel) {
  const float a = 0.17883277;
  const float b = 0.28466892;
  const float c = 0.55991073;

  return linearChannel <= 1.0 / 12.0 ? sqrt(3.0 * linearChannel)
                                     : a * log(12.0 * linearChannel - b) + c;
}

// BT.2100 / BT.2020 HLG OETF.
vec3 hlgOetf(vec3 linearColor) {
  return vec3(hlgOetfSingleChannel(linearColor.r),
              hlgOetfSingleChannel(linearColor.g),
              hlgOetfSingleChannel(linearColor.b));
}

// BT.2100 / BT.2020, PQ / ST2084 OETF.
vec3 pqOetf(vec3 linearColor) {
  const float m1 = (2610.0 / 16384.0);
  const float m2 = (2523.0 / 4096.0) * 128.0;
  const float c1 = (3424.0 / 4096.0);
  const float c2 = (2413.0 / 4096.0) * 32.0;
  const float c3 = (2392.0 / 4096.0) * 32.0;

  vec3 temp = pow(linearColor, vec3(m1));
  temp = (c1 + c2 * temp) / (1.0 + c3 * temp);
  return pow(temp, vec3(m2));
}

// BT.709 gamma 2.2 OETF for one channel.
float gamma22OetfSingleChannel(float linearChannel) {
  return pow(linearChannel, (1.0 / 2.2));
}

// BT.709 gamma 2.2 OETF.
vec3 gamma22Oetf(vec3 linearColor) {
  return vec3(gamma22OetfSingleChannel(linearColor.r),
              gamma22OetfSingleChannel(linearColor.g),
              gamma22OetfSingleChannel(linearColor.b));
}

// Applies the appropriate OETF to convert linear optical signals to nonlinear
// electrical signals. Input and output are both normalized to [0, 1].
vec3 applyOetf(vec3 linearColor) {
  if (uOutputColorTransfer == COLOR_TRANSFER_ST2084) {
    return pqOetf(linearColor);
  } else if (uOutputColorTransfer == COLOR_TRANSFER_HLG) {
    return hlgOetf(linearColor);
  } else if (uOutputColorTransfer == COLOR_TRANSFER_GAMMA_2_2) {
    return gamma22Oetf(linearColor);
  } else if (uOutputColorTransfer == COLOR_TRANSFER_LINEAR) {
    return linearColor;
  } else {
    // Output blue as an obviously visible error.
    return vec3(0.0, 0.0, 1.0);
  }
}

vec3 yuvToRgb(vec3 yuv) {
  const vec3 yuvOffset = vec3(0.0625, 0.5, 0.5);
  return clamp(uYuvToRgbColorTransform * (yuv - yuvOffset), 0.0, 1.0);
}

vec4 convert_to_rgb(vec4 tex) {
  vec3 srcYuv = tex.xyz;
  vec3 opticalColorBt2020 = applyEotf(yuvToRgb(srcYuv));
  vec4 opticalColor =
      (uApplyHdrToSdrToneMapping == 1)
          ? vec4(applyBt2020ToBt709Ootf(opticalColorBt2020), 1.0)
          : vec4(opticalColorBt2020, 1.0);
  vec4 transformedColors = opticalColor;
  return vec4(applyOetf(transformedColors.rgb), 1.0);
})_";

static inline QString bt2020shader(const Video::ImageFormat& d)
{
  QString shader;
  shader.reserve(8000);

  shader += SCORE_GFX_CONVERT_BT2020_TO_RGB_HEADER;
  if(d.color_range == AVCOL_RANGE_MPEG)
    shader += SCORE_GFX_CONVERT_BT2020_TO_RGB_HEADER_LIMITED_RANGE;
  else
    shader += SCORE_GFX_CONVERT_BT2020_TO_RGB_HEADER_FULL_RANGE;

  if(d.color_trc == AVCOL_TRC_SMPTE2084)
    shader += "const int uInputColorTransfer = COLOR_TRANSFER_ST2084; \n";
  else if(d.color_trc == AVCOL_TRC_GAMMA22)
    shader += "const int uInputColorTransfer = COLOR_TRANSFER_GAMMA_2_2; \n";
  else if(d.color_trc == AVCOL_TRC_LINEAR)
    shader += "const int uInputColorTransfer = COLOR_TRANSFER_LINEAR; \n";
  else if(d.color_trc == AVCOL_TRC_ARIB_STD_B67)
    shader += "const int uInputColorTransfer = COLOR_TRANSFER_HLG; \n";
  else
    shader += "const int uInputColorTransfer = COLOR_TRANSFER_GAMMA_2_2; \n";

  shader += "const int  uOutputColorTransfer = COLOR_TRANSFER_GAMMA_2_2; \n";

  shader += SCORE_GFX_CONVERT_BT2020_TO_RGB;

  return shader;
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
      return bt2020shader(d);
    case AVCOL_SPC_BT2020_CL:
      // NOTE: BT.2020 constant luminance requires a different decoding path.
      // In practice CL content is extremely rare; treating as NCL is a
      // reasonable approximation.
      return bt2020shader(d);
    case AVCOL_SPC_SMPTE2085:
      return bt2020shader(d);
    case AVCOL_SPC_CHROMA_DERIVED_NCL:
      return bt2020shader(d);
    case AVCOL_SPC_CHROMA_DERIVED_CL:
      return bt2020shader(d);
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

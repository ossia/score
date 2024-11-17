#pragma once
#include <Video/VideoInterface.hpp>

// See softpixel.com/~cwright/programming/colorspace/yuv
//
// https://github.com/vlc-qt/vlc-qt/blob/master/src/qml/painter/GlPainter.cpp#L48
namespace score::gfx
{
/* OLD code for reference
// 601
const vec3 R_cf = vec3(1.164383,  0.000000,  1.596027);
const vec3 G_cf = vec3(1.164383, -0.391762, -0.812968);
const vec3 B_cf = vec3(1.164383,  2.017232,  0.000000);
const vec3 offset = vec3(-0.0625, -0.5, -0.5);
yuv += offset;
r = dot(yuv, R_cf)
g = dot(yuv, G_cf)
b = dot(yuv, B_cf)

// OR (cf wiki https://en.wikipedia.org/wiki/YCbCr) 
const vec3 offset = vec3(0., -0.5, -0.5);
const mat3 coeff = mat3(1.0   ,  1.0   , 1.0,
                        0.0   , -0.3455, 1.7790,
                        1.4075, -0.7169, 0.);


// https://github.com/vlc-qt/vlc-qt/blob/master/src/qml/painter/GlPainter.cpp#L48
const mat4 bt601 = mat4(
                    1.164383561643836,  1.164383561643836, 1.164383561643836, 0.0,
                    0.0              , -0.391762290094914, 2.017232142857142, 0.0,
                    1.596026785714286, -0.812967647237771, 0.0              , 0.0,
                   -0.874202217873451, 0.531667823499146, -1.085630789302022, 1.0);
const mat4 bt709 = mat4(
                   1.164383561643836 , 1.164383561643836 , 1.164383561643836 , 0.0,
                   0.0               , -0.21324861427373 , 2.112401785714286 , 0.0,
                   1.792741071428571 , -0.532909328559444, 0.0               , 0.0,
                   -0.972945075016308, 0.301482665475862 , -1.133402217873451, 1.0);



  // yuv += offset;

  // fragColor = vec4(0.0, 0.0, 0.0, 1.0);
  // fragColor.r = dot(yuv, R_cf);
  // fragColor.g = dot(yuv, G_cf);
  // fragColor.b = dot(yuv, B_cf);

OR


    fragColor = vec4(coeff * (vec3(y,u,v) + offset), 1);

OR

  vec4 tex = texture(u_tex, v_texcoord);
  float y = tex.r;
  float u = tex.g - 0.5;
  float v = tex.a - 0.5;

  vec4 yuv = vec4(y,u,v, 1.0);

  // BT402 (wikipedia coeffs)
  fragColor.r = y + 1.13983 * v;
  fragColor.g = y - 0.39465 * u - 0.58060 * v;
  fragColor.b = y + 2.03211 * u;
  fragColor.a = 1.0;

OR

   // For U0 Y0 V0 Y1 macropixel, lookup Y0 or Y1 based on whether
   // the original texture x coord is even or odd.
   vec4 uyvy = texture(u_tex, v_texcoord);
   float Y;
   if (fract(floor(v_texcoord.x * renderer.renderSize.x + 0.5) / 2.0) > 0.0)
       Y = uyvy.a;       // odd so choose Y1
   else
       Y = uyvy.g;       // even so choose Y0
   float Cb = uyvy.r;
   float Cr = uyvy.b;

   vec4 yuv = vec4(Y, Cb, Cr, 1.0);
// FIXME 601
   fragColor = bt709 * yuv;
*/

#define SCORE_GFX_RGB_MATRIX \
  "mat4(\
    1., 0., 0., 0.0,\n\
    0., 1., 0., 0.0,\n\
    0., 0., 1., 0.0,\n\
    0., 0., 0., 1.0)\n"

#define SCORE_GFX_BT601_MATRIX \
  "mat4(\
    1.164383561643836,  1.164383561643836, 1.164383561643836, 0.0,\n\
    0.0              , -0.391762290094914, 2.017232142857142, 0.0,\n\
    1.596026785714286, -0.812967647237771, 0.0              , 0.0,\n\
    -0.874202217873451, 0.531667823499146, -1.085630789302022, 1.0)\n"

#define SCORE_GFX_BT709_MATRIX \
  "mat4(\n\
                    1.164383561643836 , 1.164383561643836 , 1.164383561643836 , 0.0,\n\
                    0.0               , -0.21324861427373 , 2.112401785714286 , 0.0,\n\
                    1.792741071428571 , -0.532909328559444, 0.0               , 0.0,\n\
                    -0.972945075016308, 0.301482665475862 , -1.133402217873451, 1.0)\n"

#define SCORE_GFX_BT2020_TO_709_MATRIX \
  "mat4(\n\
    1.660491f, -0.587641f, -0.072850f, 0.000000f,\n\
    -0.124550f, 1.132900f, -0.008349f, 0.000000f,\n\
    -0.018151f, -0.100579f, 1.118730f, 0.000000f,\n\
    0.000000f, 0.000000f, 0.000000f, 1.000000f\n\
    )\n"

#define SCORE_GFX_BT2020_MATRIX SCORE_GFX_BT709_MATRIX
#define SCORE_GFX_CONVERT_BT601_TO_RGB                     \
  "const mat4 conversion_matrix = " SCORE_GFX_BT601_MATRIX \
  ";\n"                                                    \
  "vec4 convert_to_rgb(vec4 tex) { return conversion_matrix * tex; }\n"

#define SCORE_GFX_CONVERT_BT709_TO_RGB                     \
  "const mat4 conversion_matrix = " SCORE_GFX_BT709_MATRIX \
  ";\n"                                                    \
  "vec4 convert_to_rgb(vec4 tex) { return conversion_matrix * tex; }\n"

// https://github.com/cloudveiltech/CloudVeilMessenger/blob/4bb018f58320fb453a40e0add2af061b9f9300ca/TMessagesProj/src/main/res/raw/yuv_hlg2rgb.glsl#L65
// https://github.com/Themaister/Granite/blob/7543863d2a101faf45f897d164b72037ae98ff74/video/ffmpeg_decode.cpp#L573

// BT2020 code comes from:
// https://github.com/google/ExoPlayer/blob/dd430f7053a1a3958deea3ead6a0565150c06bfc/library/effect/src/main/assets/shaders/fragment_shader_transformation_external_yuv_es3.glsl#L25
// https://github.com/androidx/media/blob/c35a9d62baec57118ea898e271ac66819399649b/libraries/effect/src/main/java/androidx/media3/effect/DefaultShaderProgram.java#L315
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
  // Specification:
  // https://www.khronos.org/registry/DataFormat/specs/1.3/dataformat.1.3.inline.html#TRANSFER_HLG
  // Reference implementation:
  // https://cs.android.com/android/platform/superproject/+/master:frameworks/native/libs/renderengine/gl/ProgramCache.cpp;l=265-279;drc=de09f10aa504fd8066370591a00c9ff1cafbb7fa
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
  // Specification:
  // https://registry.khronos.org/DataFormat/specs/1.3/dataformat.1.3.inline.html#TRANSFER_PQ
  // Reference implementation:
  // https://cs.android.com/android/platform/superproject/+/master:frameworks/native/libs/renderengine/gl/ProgramCache.cpp;l=250-263;drc=de09f10aa504fd8066370591a00c9ff1cafbb7fa
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
  // Reference ("HLG Reference OOTF" section):
  // https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2100-2-201807-I!!PDF-E.pdf
  // Matrix values based on computeXYZMatrix(BT2020Primaries, BT2020WhitePoint)
  // https://cs.android.com/android/platform/superproject/+/master:frameworks/base/libs/hwui/utils/HostColorSpace.cpp;l=200-232;drc=86bd214059cd6150304888a285941bf74af5b687
  const mat3 RGB_TO_XYZ_BT2020 =
      mat3(0.63695805f, 0.26270021f, 0.00000000f, 0.14461690f, 0.67799807f,
           0.02807269f, 0.16888098f, 0.05930172f, 1.06098506f);
  // Matrix values based on computeXYZMatrix(BT709Primaries, BT709WhitePoint)
  const mat3 XYZ_TO_RGB_BT709 =
      mat3(3.24096994f, -0.96924364f, 0.05563008f, -1.53738318f, 1.87596750f,
           -0.20397696f, -0.49861076f, 0.04155506f, 1.05697151f);
  // hlgGamma is 1.2 + 0.42 * log10(nominalPeakLuminance/1000);
  // nominalPeakLuminance was selected to use a 500 as a typical value, used
  // in
  // https://cs.android.com/android/platform/superproject/+/master:frameworks/native/libs/tonemap/tonemap.cpp;drc=7a577450e536aa1e99f229a0cb3d3531c82e8a8d;l=62,
  // b/199162498#comment35, and
  // https://www.microsoft.com/applied-sciences/uploads/projects/investigation-of-hdr-vs-tone-mapped-sdr/investigation-of-hdr-vs-tone-mapped-sdr.pdf.
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
  // Specification:
  // https://www.khronos.org/registry/DataFormat/specs/1.3/dataformat.1.3.inline.html#TRANSFER_HLG
  // Reference implementation:
  // https://cs.android.com/android/platform/superproject/+/master:frameworks/native/libs/renderengine/gl/ProgramCache.cpp;l=529-543;drc=de09f10aa504fd8066370591a00c9ff1cafbb7fa
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
  // Specification:
  // https://registry.khronos.org/DataFormat/specs/1.3/dataformat.1.3.inline.html#TRANSFER_PQ
  // Reference implementation:
  // https://cs.android.com/android/platform/superproject/+/master:frameworks/native/libs/renderengine/gl/ProgramCache.cpp;l=514-527;drc=de09f10aa504fd8066370591a00c9ff1cafbb7fa
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
  // Reference:
  // https://developer.android.com/reference/android/hardware/DataSpace#TRANSFER_GAMMA2_2
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
static inline QString colorMatrix(const Video::ImageFormat& d)
{
  // qDebug() << "colorMatrix:" << magic_enum::enum_name(d.color_space).data()
  //          << magic_enum::enum_name(d.color_primaries).data()
  //          << magic_enum::enum_name(d.color_range).data()
  //          << magic_enum::enum_name(d.color_trc).data()
  //          << magic_enum::enum_name(d.chroma_location).data();
  // FIXME handle full color range for 601 / 709

  switch(d.color_space)
  {
    case AVCOL_SPC_RGB: ///< order of coefficients is actually GBR, also IEC 61966-2-1 (sRGB), YZX and ST 428-1
      return "vec4 convert_to_rgb(vec4 tex) { return tex; }";
    case AVCOL_SPC_BT709: ///< also ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / derived in SMPTE RP 177 Annex B
      return SCORE_GFX_CONVERT_BT709_TO_RGB;
    case AVCOL_SPC_FCC: ///< FCC Title 47 Code of Federal Regulations 73.682 (a)(20)
      return SCORE_GFX_CONVERT_BT601_TO_RGB; // FIXME
    case AVCOL_SPC_BT470BG: ///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM / IEC 61966-2-4 xvYCC601
      return SCORE_GFX_CONVERT_BT601_TO_RGB;
    case AVCOL_SPC_SMPTE170M: ///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC / functionally identical to above
      return SCORE_GFX_CONVERT_BT601_TO_RGB;
    case AVCOL_SPC_SMPTE240M: ///< derived from 170M primaries and D65 white point, 170M is derived from BT470 System M's primaries
      // FIXME see wikipedia
      return SCORE_GFX_CONVERT_BT709_TO_RGB;
    case AVCOL_SPC_YCGCO: ///< used by Dirac / VC-2 and H.264 FRext, see ITU-T SG16
      return SCORE_GFX_CONVERT_BT709_TO_RGB; // FIXME
    case AVCOL_SPC_BT2020_NCL: ///< ITU-R BT2020 non-constant luminance system
      return bt2020shader(d);
    case AVCOL_SPC_BT2020_CL:                 ///< ITU-R BT2020 constant luminance system
      return bt2020shader(d);                 // FIXME
    case AVCOL_SPC_SMPTE2085:                 ///< SMPTE 2085, Y'D'zD'x
      return bt2020shader(d);                 // FIXME
    case AVCOL_SPC_CHROMA_DERIVED_NCL: ///< Chromaticity-derived non-constant luminance system
      return bt2020shader(d);          // FIXME
    case AVCOL_SPC_CHROMA_DERIVED_CL: ///< Chromaticity-derived constant luminance system
      return bt2020shader(d);         // FIXME
    case AVCOL_SPC_ICTCP:                     ///< ITU-R BT.2100-0, ICtCp
      return bt2020shader(d);                 // FIXME

    default:
    case AVCOL_SPC_NB:
    case AVCOL_SPC_UNSPECIFIED:
    case AVCOL_SPC_RESERVED: ///< reserved for future use by ITU-T and ISO/IEC just like 15-255 are
      break;
  }
  return SCORE_GFX_BT709_MATRIX; // FIXME
}
}

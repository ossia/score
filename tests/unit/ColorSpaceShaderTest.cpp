// score::gfx::colorMatrix() and score::gfx::tonemapShader(): the GLSL the video
// decoders splice into every YUV sampling shader.
//
// Both are pure functions of a Video::ImageFormat, so the whole decision tree —
// H.273 matrix coefficients, colour primaries, range, transfer characteristic,
// output format and tone mapper — is checkable without a GPU, a decoder or a
// video file. This is the format/state matrix, not a rendering test: it asserts
// which generator each descriptor reaches, that the mutually exclusive
// generators really are mutually exclusive, and that the numeric helpers behind
// them return the documented values.

#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/Tonemap.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <iterator>
#include <set>

using Catch::Approx;
using score::gfx::colorMatrix;

namespace Catch
{
// Without this the generated programs stringify as a wall of "{?}".
template <>
struct StringMaker<QString>
{
  static std::string convert(const QString& s)
  {
    const QString head = s.left(400);
    return (s.size() > 400 ? head + QStringLiteral("... [%1 chars]").arg(s.size())
                           : head)
        .toStdString();
  }
};
}

namespace
{
Video::ImageFormat fmt(
    AVColorSpace space, AVColorRange range = AVCOL_RANGE_MPEG,
    AVColorPrimaries pri = AVCOL_PRI_UNSPECIFIED,
    AVColorTransferCharacteristic trc = AVCOL_TRC_UNSPECIFIED)
{
  Video::ImageFormat d;
  d.width = 1920;
  d.height = 1080;
  d.color_space = space;
  d.color_range = range;
  d.color_primaries = pri;
  d.color_trc = trc;
  return d;
}

bool has(const QString& s, const char* needle)
{
  return s.contains(QLatin1String(needle));
}
}

// -----------------------------------------------------------------------------
// Tonemap.hpp
// -----------------------------------------------------------------------------

TEST_CASE("Auto tonemap resolution", "[gfx][video][colorspace]")
{
  // The helper takes an int precisely so it does not need libavutil; the values
  // it hard-codes must still agree with the enum it stands in for.
  CHECK(score::gfx::resolveAutoTonemap(AVCOL_TRC_SMPTE2084) == Video::BT_2390);
  CHECK(score::gfx::resolveAutoTonemap(AVCOL_TRC_ARIB_STD_B67) == Video::Clamp);
  CHECK(score::gfx::resolveAutoTonemap(AVCOL_TRC_BT709) == Video::Clamp);
  CHECK(score::gfx::resolveAutoTonemap(AVCOL_TRC_LINEAR) == Video::Clamp);
  CHECK(score::gfx::resolveAutoTonemap(-1) == Video::Clamp);

  SECTION("resolvedTonemap only rewrites Auto")
  {
    auto d = fmt(AVCOL_SPC_BT2020_NCL, AVCOL_RANGE_MPEG, AVCOL_PRI_BT2020,
                 AVCOL_TRC_SMPTE2084);
    d.tonemap = Video::Auto;
    CHECK(score::gfx::resolvedTonemap(d) == Video::BT_2390);

    d.tonemap = Video::Hable;
    CHECK(score::gfx::resolvedTonemap(d) == Video::Hable);

    d.color_trc = AVCOL_TRC_BT709;
    d.tonemap = Video::Auto;
    CHECK(score::gfx::resolvedTonemap(d) == Video::Clamp);
  }
}

TEST_CASE("Tonemapper gamut classification", "[gfx][video][colorspace]")
{
  // Luminance-based tonemappers run before the BT.2020->BT.709 gamut matrix;
  // per-channel ones must run after it. Getting this backwards is a hue shift,
  // not a crash, so it is pinned here.
  CHECK(score::gfx::isLuminanceBasedTonemap(Video::BT_2390));
  CHECK(score::gfx::isLuminanceBasedTonemap(Video::BT_2446));
  CHECK(score::gfx::isLuminanceBasedTonemap(Video::Reinhard));

  CHECK_FALSE(score::gfx::isLuminanceBasedTonemap(Video::Hable));
  CHECK_FALSE(score::gfx::isLuminanceBasedTonemap(Video::ACES2));
  CHECK_FALSE(score::gfx::isLuminanceBasedTonemap(Video::AgX));
  CHECK_FALSE(score::gfx::isLuminanceBasedTonemap(Video::PBR_Neutral));
  CHECK_FALSE(score::gfx::isLuminanceBasedTonemap(Video::Clamp));
  CHECK_FALSE(score::gfx::isLuminanceBasedTonemap(Video::Auto));
}

TEST_CASE("Tonemap shader generation", "[gfx][video][colorspace]")
{
  SECTION("the constants block carries the two peak luminances")
  {
    const auto c = score::gfx::tonemapConstants(1000.0f, 203.0f);
    CHECK(has(c, "const float contentPeakNits = 1000.0;"));
    CHECK(has(c, "const float sdrPeakNits = 203.0;"));

    const auto c2 = score::gfx::tonemapConstants(4000.0f, 100.0f);
    CHECK(has(c2, "const float contentPeakNits = 4000.0;"));
    CHECK(has(c2, "const float sdrPeakNits = 100.0;"));
  }

  SECTION("every mode defines exactly one tonemap() and its own body")
  {
    const struct
    {
      Video::Tonemap mode;
      const char* marker;
    } cases[] = {
        {Video::BT_2390, "bt2390_eetf"},
        {Video::BT_2446, "lumaCoeff"},
        {Video::Reinhard, "whitePoint"},
        {Video::Hable, "hableCurve"},
        {Video::ACES2, "ACESInputMat"},
        {Video::AgX, "agxDefaultContrastApprox"},
        {Video::PBR_Neutral, "pbrNeutralToneMapping"},
        {Video::Clamp, "return clamp(color, 0.0, 1.0);"},
    };

    std::set<QString> bodies;
    for(const auto& c : cases)
    {
      const auto s = score::gfx::tonemapShader(c.mode);
      INFO("tonemap mode " << int(c.mode));
      CHECK(has(s, "vec3 tonemap(vec3 color)"));
      CHECK(s.count(QString::fromLatin1("vec3 tonemap(vec3 color)")) == 1);
      CHECK(has(s, c.marker));
      CHECK(has(s, "const float contentPeakNits"));
      bodies.insert(s);
    }
    // No two modes emit the same program.
    CHECK(bodies.size() == std::size(cases));
  }

  SECTION("Auto falls back to Clamp rather than emitting nothing")
  {
    CHECK(
        score::gfx::tonemapShader(Video::Auto)
        == score::gfx::tonemapShader(Video::Clamp));
  }

  SECTION("the peak luminances reach the emitted program")
  {
    const auto s = score::gfx::tonemapShader(Video::BT_2390, 4000.0f, 120.0f);
    CHECK(has(s, "contentPeakNits = 4000.0"));
    CHECK(has(s, "sdrPeakNits = 120.0"));
  }
}

// -----------------------------------------------------------------------------
// ColorSpace.hpp — numeric helpers
// -----------------------------------------------------------------------------

TEST_CASE("BT.2020 content peak luminance", "[gfx][video][colorspace]")
{
  SECTION("defaults come from the transfer function")
  {
    auto d = fmt(AVCOL_SPC_BT2020_NCL);
    d.color_trc = AVCOL_TRC_SMPTE2084;
    CHECK(score::gfx::bt2020_contentPeakNits(d) == Approx(1000.0f));
    d.color_trc = AVCOL_TRC_ARIB_STD_B67;
    CHECK(score::gfx::bt2020_contentPeakNits(d) == Approx(1000.0f));
    d.color_trc = AVCOL_TRC_BT709;
    CHECK(score::gfx::bt2020_contentPeakNits(d) == Approx(100.0f));
    d.color_trc = AVCOL_TRC_LINEAR;
    CHECK(score::gfx::bt2020_contentPeakNits(d) == Approx(100.0f));
  }

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 3, 100)
#if __has_include(<libavutil/mastering_display_metadata.h>)
  SECTION("MaxCLL is used when it is plausible")
  {
    auto d = fmt(AVCOL_SPC_BT2020_NCL);
    d.color_trc = AVCOL_TRC_SMPTE2084;

    AVContentLightMetadata cll{};
    cll.MaxCLL = 4000;
    d.content_light = cll;
    CHECK(score::gfx::bt2020_contentPeakNits(d) == Approx(4000.0f));

    // Out of the [100, 10000] plausibility window: ignored.
    d.content_light->MaxCLL = 12;
    CHECK(score::gfx::bt2020_contentPeakNits(d) == Approx(1000.0f));
    d.content_light->MaxCLL = 50000;
    CHECK(score::gfx::bt2020_contentPeakNits(d) == Approx(1000.0f));
  }

  SECTION("mastering display luminance is the fallback")
  {
    auto d = fmt(AVCOL_SPC_BT2020_NCL);
    d.color_trc = AVCOL_TRC_SMPTE2084;
    d.mastering_display.has_luminance = 1;
    d.mastering_display.max_luminance = AVRational{600, 1};
    CHECK(score::gfx::bt2020_contentPeakNits(d) == Approx(600.0f));

    // A zero denominator must not divide.
    d.mastering_display.max_luminance = AVRational{600, 0};
    CHECK(score::gfx::bt2020_contentPeakNits(d) == Approx(1000.0f));

    // has_luminance unset means the field is meaningless.
    d.mastering_display.has_luminance = 0;
    d.mastering_display.max_luminance = AVRational{600, 1};
    CHECK(score::gfx::bt2020_contentPeakNits(d) == Approx(1000.0f));
  }

  SECTION("MaxCLL wins over the mastering display")
  {
    auto d = fmt(AVCOL_SPC_BT2020_NCL);
    d.color_trc = AVCOL_TRC_SMPTE2084;
    AVContentLightMetadata cll{};
    cll.MaxCLL = 4000;
    d.content_light = cll;
    d.mastering_display.has_luminance = 1;
    d.mastering_display.max_luminance = AVRational{600, 1};
    CHECK(score::gfx::bt2020_contentPeakNits(d) == Approx(4000.0f));
  }
#endif
#endif
}

TEST_CASE("BT.2020 EOTF normalisation factor", "[gfx][video][colorspace]")
{
  auto d = fmt(AVCOL_SPC_BT2020_NCL);

  // PQ's EOTF is absolute: 1.0 is 10000 nits, so the signal has to be rescaled
  // into "1.0 == content peak" space.
  d.color_trc = AVCOL_TRC_SMPTE2084;
  CHECK(score::gfx::bt2020_eotfToNormalizedFactor(d) == Approx(10.0f));

  // HLG is scene-relative already.
  d.color_trc = AVCOL_TRC_ARIB_STD_B67;
  CHECK(score::gfx::bt2020_eotfToNormalizedFactor(d) == Approx(1.0f));

  d.color_trc = AVCOL_TRC_BT709;
  CHECK(score::gfx::bt2020_eotfToNormalizedFactor(d) == Approx(1.0f));

  SECTION("the PQ factor tracks the content peak")
  {
    d.color_trc = AVCOL_TRC_SMPTE2084;
    const float base = score::gfx::bt2020_eotfToNormalizedFactor(d);
    CHECK(base * score::gfx::bt2020_contentPeakNits(d) == Approx(10000.0f));
  }

  // BT.2408 reference white.
  CHECK(score::gfx::bt2020_hlgDisplayPeakNits(d) == Approx(203.0f));
}

TEST_CASE("BT.2020 matrix and EOTF selection", "[gfx][video][colorspace]")
{
  SECTION("range picks the YUV offset")
  {
    QString limited, full;
    auto d = fmt(AVCOL_SPC_BT2020_NCL, AVCOL_RANGE_MPEG);
    score::gfx::bt2020_appendYuvMatrix(limited, d);
    d.color_range = AVCOL_RANGE_JPEG;
    score::gfx::bt2020_appendYuvMatrix(full, d);

    CHECK(has(limited, "yuvOffset = vec3(0.0625, 0.5, 0.5)"));
    CHECK(has(full, "yuvOffset = vec3(0.0, 0.5, 0.5)"));
    CHECK(limited != full);

    // Anything that is not explicitly limited is treated as full.
    QString unspecified;
    d.color_range = AVCOL_RANGE_UNSPECIFIED;
    score::gfx::bt2020_appendYuvMatrix(unspecified, d);
    CHECK(unspecified == full);
  }

  SECTION("transfer characteristic picks the EOTF")
  {
    auto emit = [](AVColorTransferCharacteristic trc) {
      QString s;
      auto d = fmt(AVCOL_SPC_BT2020_NCL);
      d.color_trc = trc;
      score::gfx::bt2020_appendEotf(s, d);
      return s;
    };

    const auto pq = emit(AVCOL_TRC_SMPTE2084);
    const auto hlg = emit(AVCOL_TRC_ARIB_STD_B67);
    const auto linear = emit(AVCOL_TRC_LINEAR);
    const auto gamma = emit(AVCOL_TRC_BT709);

    CHECK(has(hlg, "hlgEotfSingle"));
    CHECK(has(linear, "return v;"));
    CHECK(has(gamma, "vec3(2.2)"));
    CHECK(std::set<QString>{pq, hlg, linear, gamma}.size() == 4);

    // Anything unrecognised gets the gamma 2.2 fallback rather than nothing.
    CHECK(emit(AVCOL_TRC_UNSPECIFIED) == gamma);
    CHECK(emit(AVColorTransferCharacteristic(-1)) == gamma);
  }
}

// -----------------------------------------------------------------------------
// ColorSpace.hpp — colorMatrix() dispatch
// -----------------------------------------------------------------------------

TEST_CASE("colorMatrix always produces a converter", "[gfx][video][colorspace]")
{
  for(int space = -1; space < int(AVCOL_SPC_NB); space++)
    for(auto range : {AVCOL_RANGE_MPEG, AVCOL_RANGE_JPEG, AVCOL_RANGE_UNSPECIFIED})
    {
      auto d = fmt(AVColorSpace(space), range);
      const auto s = colorMatrix(d);
      INFO("color_space " << space << " range " << int(range));
      CHECK(!s.isEmpty());
      CHECK(has(s, "vec4 convert_to_rgb(vec4 tex)"));
    }
}

TEST_CASE("colorMatrix SDR matrices", "[gfx][video][colorspace]")
{
  SECTION("RGB is a passthrough")
  {
    const auto s = colorMatrix(fmt(AVCOL_SPC_RGB));
    CHECK(has(s, "return tex;"));
    CHECK_FALSE(has(s, "conversion_matrix"));
  }

  SECTION("each matrix coefficient set maps to its own matrix")
  {
    const struct
    {
      AVColorSpace space;
      const char* limited;
      const char* full;
    } cases[] = {
        {AVCOL_SPC_BT709, SCORE_GFX_CONVERT_BT709_LIMITED_TO_RGB,
         SCORE_GFX_CONVERT_BT709_FULL_TO_RGB},
        {AVCOL_SPC_FCC, SCORE_GFX_CONVERT_FCC_LIMITED_TO_RGB,
         SCORE_GFX_CONVERT_FCC_FULL_TO_RGB},
        {AVCOL_SPC_BT470BG, SCORE_GFX_CONVERT_BT601_LIMITED_TO_RGB,
         SCORE_GFX_CONVERT_BT601_FULL_TO_RGB},
        {AVCOL_SPC_SMPTE170M, SCORE_GFX_CONVERT_BT601_LIMITED_TO_RGB,
         SCORE_GFX_CONVERT_BT601_FULL_TO_RGB},
        {AVCOL_SPC_SMPTE240M, SCORE_GFX_CONVERT_SMPTE240M_LIMITED_TO_RGB,
         SCORE_GFX_CONVERT_SMPTE240M_FULL_TO_RGB},
        {AVCOL_SPC_YCGCO, SCORE_GFX_CONVERT_YCGCO_LIMITED_TO_RGB,
         SCORE_GFX_CONVERT_YCGCO_FULL_TO_RGB},
    };

    for(const auto& c : cases)
    {
      INFO("color_space " << int(c.space));
      CHECK(colorMatrix(fmt(c.space, AVCOL_RANGE_MPEG)) == QString(c.limited));
      CHECK(colorMatrix(fmt(c.space, AVCOL_RANGE_JPEG)) == QString(c.full));
      // Only AVCOL_RANGE_JPEG counts as full range.
      CHECK(
          colorMatrix(fmt(c.space, AVCOL_RANGE_UNSPECIFIED)) == QString(c.limited));
    }
  }

  SECTION("BT.709 coefficients over P3 primaries route to the P3 pipeline")
  {
    const auto p3 = colorMatrix(
        fmt(AVCOL_SPC_BT709, AVCOL_RANGE_MPEG, AVCOL_PRI_SMPTE432));
    const auto dci = colorMatrix(
        fmt(AVCOL_SPC_BT709, AVCOL_RANGE_MPEG, AVCOL_PRI_SMPTE431));
    const auto plain = colorMatrix(fmt(AVCOL_SPC_BT709, AVCOL_RANGE_MPEG));

    CHECK(p3 != plain);
    CHECK(p3 == dci);
    CHECK(plain == QString(SCORE_GFX_CONVERT_BT709_LIMITED_TO_RGB));
  }
}

TEST_CASE("colorMatrix HDR pipelines", "[gfx][video][colorspace]")
{
  const auto pri = AVCOL_PRI_BT2020;
  const auto trc = AVCOL_TRC_SMPTE2084;

  const auto ncl = colorMatrix(fmt(AVCOL_SPC_BT2020_NCL, AVCOL_RANGE_MPEG, pri, trc));
  const auto cl = colorMatrix(fmt(AVCOL_SPC_BT2020_CL, AVCOL_RANGE_MPEG, pri, trc));
  const auto s2085
      = colorMatrix(fmt(AVCOL_SPC_SMPTE2085, AVCOL_RANGE_MPEG, pri, trc));
  const auto ictcp = colorMatrix(fmt(AVCOL_SPC_ICTCP, AVCOL_RANGE_MPEG, pri, trc));

  SECTION("constant luminance is approximated by the NCL path")
  {
    CHECK(cl == ncl);
  }

  SECTION("the three BT.2100 encodings are distinct programs")
  {
    CHECK(std::set<QString>{ncl, s2085, ictcp}.size() == 3);
    CHECK(has(ictcp, "ictcpToLms"));
    CHECK_FALSE(has(ncl, "ictcpToLms"));
  }

  SECTION("ICtCp picks its inverse matrix from the transfer function")
  {
    const auto hlg = colorMatrix(
        fmt(AVCOL_SPC_ICTCP, AVCOL_RANGE_MPEG, pri, AVCOL_TRC_ARIB_STD_B67));
    CHECK(hlg != ictcp);
    CHECK(has(hlg, "ictcpToLms"));

    // The matrix choice is the only part of the program the transfer function
    // drives here (the EOTF, peak and tonemapper differ too, so the whole
    // programs are not comparable). Assert on the helper itself.
    auto inverse = [&](AVColorTransferCharacteristic trc) {
      QString s;
      score::gfx::ictcp_appendInverseMatrix(
          s, fmt(AVCOL_SPC_ICTCP, AVCOL_RANGE_MPEG, pri, trc));
      return s;
    };
    const auto pqInverse = inverse(AVCOL_TRC_SMPTE2084);
    CHECK(has(pqInverse, "ictcpToLms"));
    CHECK(inverse(AVCOL_TRC_ARIB_STD_B67) != pqInverse);
    // Anything that is not HLG defaults to the PQ inverse.
    CHECK(inverse(AVCOL_TRC_UNSPECIFIED) == pqInverse);
    CHECK(inverse(AVCOL_TRC_BT709) == pqInverse);
  }

  SECTION("each output format is its own program")
  {
    std::set<QString> programs;
    for(auto out : {Video::SDR, Video::Passthrough, Video::Linear, Video::Normalized})
    {
      auto d = fmt(AVCOL_SPC_BT2020_NCL, AVCOL_RANGE_MPEG, pri, trc);
      d.output_format = out;
      const auto s = colorMatrix(d);
      INFO("output_format " << int(out));
      CHECK(has(s, "vec4 convert_to_rgb(vec4 tex)"));
      programs.insert(s);
    }
    CHECK(programs.size() == 4);
  }

  SECTION("the SDR path carries the resolved tonemapper")
  {
    auto d = fmt(AVCOL_SPC_BT2020_NCL, AVCOL_RANGE_MPEG, pri, trc);
    d.output_format = Video::SDR;

    d.tonemap = Video::Hable;
    CHECK(has(colorMatrix(d), "hableCurve"));

    d.tonemap = Video::ACES2;
    CHECK(has(colorMatrix(d), "ACESInputMat"));

    // Auto over PQ content resolves to BT.2390.
    d.tonemap = Video::Auto;
    CHECK(has(colorMatrix(d), "bt2390_eetf"));
  }

  SECTION("HLG in SDR runs the OOTF and branches on the tonemapper class")
  {
    auto d = fmt(
        AVCOL_SPC_BT2020_NCL, AVCOL_RANGE_MPEG, pri, AVCOL_TRC_ARIB_STD_B67);
    d.output_format = Video::SDR;

    d.tonemap = Video::BT_2390; // luminance-based: gamut convert after tonemap
    const auto lum = colorMatrix(d);
    d.tonemap = Video::Hable; // per-channel: gamut convert before tonemap
    const auto perChannel = colorMatrix(d);

    CHECK(has(lum, "applyHlgOotf"));
    CHECK(has(perChannel, "applyHlgOotf"));
    CHECK(has(lum, "hlgDisplayLw = 203.0"));
    CHECK(lum != perChannel);

    // PQ takes neither: no OOTF, and the normalisation factor is the PQ one.
    d.color_trc = AVCOL_TRC_SMPTE2084;
    const auto pq = colorMatrix(d);
    CHECK_FALSE(has(pq, "applyHlgOotf"));
    CHECK(has(pq, "eotfNormFactor = 10.000000"));
  }

  SECTION("the passthrough path applies no EOTF")
  {
    auto d = fmt(AVCOL_SPC_BT2020_NCL, AVCOL_RANGE_MPEG, pri, trc);
    d.output_format = Video::Passthrough;
    const auto s = colorMatrix(d);
    CHECK_FALSE(has(s, "applyEotf"));
    CHECK(has(s, "uYuvToRgbColorTransform"));
  }
}

TEST_CASE("colorMatrix chroma-derived coefficients", "[gfx][video][colorspace]")
{
  // H.273 MatrixCoefficients 12/13: Kr/Kb are derived from the primaries, so
  // the whole decision is delegated to color_primaries.
  auto derived = [](AVColorPrimaries pri, AVColorRange range = AVCOL_RANGE_MPEG) {
    return colorMatrix(fmt(AVCOL_SPC_CHROMA_DERIVED_NCL, range, pri));
  };

  CHECK(
      derived(AVCOL_PRI_BT2020)
      == colorMatrix(fmt(AVCOL_SPC_BT2020_NCL, AVCOL_RANGE_MPEG, AVCOL_PRI_BT2020)));
  CHECK(
      derived(AVCOL_PRI_SMPTE432)
      == colorMatrix(fmt(AVCOL_SPC_BT709, AVCOL_RANGE_MPEG, AVCOL_PRI_SMPTE432)));
  CHECK(derived(AVCOL_PRI_SMPTE431) == derived(AVCOL_PRI_SMPTE432));

  CHECK(derived(AVCOL_PRI_BT709) == QString(SCORE_GFX_CONVERT_BT709_LIMITED_TO_RGB));
  CHECK(
      derived(AVCOL_PRI_UNSPECIFIED) == QString(SCORE_GFX_CONVERT_BT709_LIMITED_TO_RGB));
  CHECK(
      derived(AVCOL_PRI_BT470BG) == QString(SCORE_GFX_CONVERT_BT601_LIMITED_TO_RGB));
  CHECK(
      derived(AVCOL_PRI_SMPTE170M) == QString(SCORE_GFX_CONVERT_BT601_LIMITED_TO_RGB));
  CHECK(
      derived(AVCOL_PRI_SMPTE240M)
      == QString(SCORE_GFX_CONVERT_SMPTE240M_LIMITED_TO_RGB));

  SECTION("range is honoured on the derived path too")
  {
    CHECK(
        derived(AVCOL_PRI_BT709, AVCOL_RANGE_JPEG)
        == QString(SCORE_GFX_CONVERT_BT709_FULL_TO_RGB));
    CHECK(
        derived(AVCOL_PRI_BT470BG, AVCOL_RANGE_JPEG)
        == QString(SCORE_GFX_CONVERT_BT601_FULL_TO_RGB));
  }

  SECTION("the constant-luminance variant is the same approximation")
  {
    for(auto pri : {AVCOL_PRI_BT2020, AVCOL_PRI_SMPTE432, AVCOL_PRI_BT709,
                    AVCOL_PRI_BT470BG, AVCOL_PRI_SMPTE240M})
    {
      INFO("primaries " << int(pri));
      CHECK(
          colorMatrix(fmt(AVCOL_SPC_CHROMA_DERIVED_CL, AVCOL_RANGE_MPEG, pri))
          == derived(pri));
    }
  }
}

TEST_CASE("colorMatrix unspecified fallback", "[gfx][video][colorspace]")
{
  // No usable matrix coefficients: fall back on the primaries, then on the
  // resolution heuristic.
  SECTION("wide-gamut primaries still route to their pipeline")
  {
    auto d = fmt(AVCOL_SPC_UNSPECIFIED, AVCOL_RANGE_MPEG, AVCOL_PRI_BT2020);
    CHECK(
        colorMatrix(d)
        == colorMatrix(fmt(AVCOL_SPC_BT2020_NCL, AVCOL_RANGE_MPEG, AVCOL_PRI_BT2020)));

    d.color_primaries = AVCOL_PRI_SMPTE432;
    CHECK(
        colorMatrix(d)
        == colorMatrix(fmt(AVCOL_SPC_BT709, AVCOL_RANGE_MPEG, AVCOL_PRI_SMPTE432)));
    d.color_primaries = AVCOL_PRI_SMPTE431;
    CHECK(
        colorMatrix(d)
        == colorMatrix(fmt(AVCOL_SPC_BT709, AVCOL_RANGE_MPEG, AVCOL_PRI_SMPTE432)));
  }

  SECTION("otherwise the width decides between BT.709 and BT.601")
  {
    auto d = fmt(AVCOL_SPC_UNSPECIFIED);

    d.width = 1280;
    CHECK(colorMatrix(d) == QString(SCORE_GFX_CONVERT_BT709_LIMITED_TO_RGB));
    d.width = 1920;
    CHECK(colorMatrix(d) == QString(SCORE_GFX_CONVERT_BT709_LIMITED_TO_RGB));
    d.width = 1279;
    CHECK(colorMatrix(d) == QString(SCORE_GFX_CONVERT_BT601_LIMITED_TO_RGB));
    d.width = 720;
    CHECK(colorMatrix(d) == QString(SCORE_GFX_CONVERT_BT601_LIMITED_TO_RGB));

    d.color_range = AVCOL_RANGE_JPEG;
    CHECK(colorMatrix(d) == QString(SCORE_GFX_CONVERT_BT601_FULL_TO_RGB));
    d.width = 1920;
    CHECK(colorMatrix(d) == QString(SCORE_GFX_CONVERT_BT709_FULL_TO_RGB));
  }

  SECTION("reserved and out-of-range values take the same fallback")
  {
    auto reserved = fmt(AVCOL_SPC_RESERVED);
    auto unspecified = fmt(AVCOL_SPC_UNSPECIFIED);
    auto negative = fmt(AVColorSpace(-1));
    CHECK(colorMatrix(reserved) == colorMatrix(unspecified));
    CHECK(colorMatrix(negative) == colorMatrix(unspecified));
  }
}

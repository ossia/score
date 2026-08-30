// Validation of Gfx/Graph/decoders/ColorSpace.hpp: the YCbCr->RGB matrices the
// video decoders paste into their fragment shaders, and the normalisation
// constants that put a sample of any bit depth on the scale those matrices
// expect.
//
// Two independent halves, because they answer two different questions:
//
//  - "derivation" checks every coefficient of every matrix against the
//    definition it comes from -- E'Y = Kr R + Kg G + Kb B, chroma as the
//    difference signals over 2(1-K), the limited-range scalings of 255/219 and
//    255/224, the full-range chroma centre of 128/255 from ITU-T H.273. It is
//    hermetic and exact: a mistyped digit anywhere in the file fails it. What
//    it cannot see is whether the definition is the one the frames actually
//    carry.
//
//  - "footage" answers that, with clips ffmpeg generates at run time. Each
//    format is encoded from a known RGB source and decoded back by ffmpeg
//    itself; the shader arithmetic is then replayed here on the same bytes
//    ffmpeg produced, and has to land within a fraction of an 8-bit code of
//    ffmpeg's own result. See kHalfCode for the bound and for what was measured
//    on each side of it. Skips when ffmpeg is not on PATH.
//
// Not covered here, and not coverable this way: the sampler state and texture
// formats the same commit changed (RGB24's sRGB flag, Y210's RG16 allocation,
// Nearest vs Linear filtering, RGBA64's data-texture routing) are GPU
// behaviour, not arithmetic. Only Y210's byte->texel layout is checkable on the
// CPU, and it is checked. The rest needs a rendering test with an RHI.

#include <Gfx/Graph/decoders/ColorSpace.hpp>

#include <QByteArray>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace
{

// ===========================================================================
//  Half 1: the matrices, against their definition
// ===========================================================================

// mat4(a, b, c, ...) with 16 scalars is column-major in GLSL, so [0..3] is the
// first column. The decoders evaluate conversion_matrix * vec4(Y, Cb, Cr, 1),
// which makes column 0 the luma coefficients, 1 the Cb coefficients, 2 the Cr
// coefficients and 3 the offset.
using Mat4 = std::array<double, 16>;

Mat4 parseMat4(std::string_view glsl)
{
  const auto open = glsl.find('(');
  REQUIRE(open != std::string_view::npos);

  Mat4 out{};
  std::size_t n = 0;
  std::size_t i = open + 1;
  while(i < glsl.size() && glsl[i] != ')')
  {
    if(std::isdigit(static_cast<unsigned char>(glsl[i])) || glsl[i] == '-'
       || glsl[i] == '+' || glsl[i] == '.')
    {
      const std::string tok{glsl.substr(i)};
      char* end{};
      const double v = std::strtod(tok.c_str(), &end);
      REQUIRE(end != tok.c_str());
      REQUIRE(n < out.size());
      out[n++] = v;
      i += static_cast<std::size_t>(end - tok.c_str());
    }
    else
    {
      ++i;
    }
  }
  REQUIRE(n == 16);
  return out;
}

enum Range
{
  Limited,
  Full
};

// ITU-T H.273 writes the full-range quantisation as
//   D'C = Clip(Round(255 * E'C + 128)),
// so the neutral chroma code is 128 and E'C = (D'C - 128) / 255. The shader
// feeds the matrix D'C / 255, hence a centre of 128/255 -- not 1/2.
constexpr double kChromaCentre = 128.0 / 255.0;

Mat4 deriveYCbCr(double Kr, double Kb, Range range)
{
  const double Kg = 1.0 - Kr - Kb;

  // Inverse of the difference-signal definition:
  //   E'Cb = (B - Y) / (2 (1 - Kb)),  E'Cr = (R - Y) / (2 (1 - Kr))
  const double r_cr = 2.0 * (1.0 - Kr);
  const double b_cb = 2.0 * (1.0 - Kb);
  const double g_cb = -2.0 * Kb * (1.0 - Kb) / Kg;
  const double g_cr = -2.0 * Kr * (1.0 - Kr) / Kg;

  // Limited range puts luma in [16, 235] and chroma in [16, 240] of 255.
  const double sy = range == Full ? 1.0 : 255.0 / 219.0;
  const double sc = range == Full ? 1.0 : 255.0 / 224.0;
  const double yoff = range == Full ? 0.0 : -(255.0 / 219.0) * (16.0 / 255.0);
  const double coff = -sc * kChromaCentre;

  return Mat4{
      sy,           sy,                        sy,           0.0,
      0.0,          g_cb * sc,                 b_cb * sc,    0.0,
      r_cr * sc,    g_cr * sc,                 0.0,          0.0,
      yoff + r_cr * coff,
      yoff + (g_cb + g_cr) * coff,
      yoff + b_cb * coff,
      1.0};
}

// R = Y - Cg + Co, G = Y + Cg, B = Y - Cg - Co. The Cg and Co terms of the
// offset do not cancel the same way in each row: they cancel in R, appear once
// in G and twice in B. A B offset equal to G's rather than twice it is the
// shape of the bug this test would have caught.
Mat4 deriveYCgCo(Range range)
{
  const double sy = range == Full ? 1.0 : 255.0 / 219.0;
  const double k = range == Full ? 1.0 : 255.0 / 224.0;
  const double yoff = range == Full ? 0.0 : -(255.0 / 219.0) * (16.0 / 255.0);
  const double coff = -k * kChromaCentre;

  return Mat4{
      sy,   sy,  sy,   0.0,
      -k,   k,   -k,   0.0,
      k,    0.0, -k,   0.0,
      yoff, yoff + coff, yoff - 2.0 * coff, 1.0};
}

void checkMatrix(const char* name, std::string_view glsl, const Mat4& expected)
{
  INFO("matrix " << name);
  const Mat4 got = parseMat4(glsl);
  for(std::size_t i = 0; i < 16; i++)
  {
    INFO("element " << i << " (column " << i / 4 << ", row " << i % 4 << ")");
    CHECK(std::abs(got[i] - expected[i]) <= 1e-14);
  }
}

// ===========================================================================
//  Half 2: real footage
// ===========================================================================

// Sixteen colours, each a solid 8x8 patch, laid out as one 128x8 strip. The
// patches are wide enough that the four interior columns of each are unaffected
// by chroma up/downsampling reaching across a patch edge, which is what keeps a
// 4:2:0 or 4:2:2 comparison meaningful.
constexpr int kPatch = 8;
constexpr std::array<std::array<int, 3>, 16> kColours{{
    {0, 0, 0},     {255, 255, 255}, {128, 128, 128}, {255, 0, 0},
    {0, 255, 0},   {0, 0, 255},     {64, 64, 64},    {192, 192, 192},
    {16, 16, 16},  {235, 235, 235}, {255, 255, 0},   {0, 255, 255},
    {255, 0, 255}, {30, 90, 200},   {200, 90, 30},   {10, 240, 120},
}};
constexpr int kWidth = kPatch * int(kColours.size());
constexpr int kHeight = kPatch;

QString ffmpegPath()
{
  return QStandardPaths::findExecutable("ffmpeg");
}

struct Fixture
{
  QTemporaryDir dir;
  QString ffmpeg;
  QString source;

  bool ok() const { return !ffmpeg.isEmpty() && dir.isValid(); }

  QString path(const QString& name) const { return dir.filePath(name); }

  bool run(const QStringList& args) const
  {
    QProcess p;
    p.start(ffmpeg, QStringList{"-v", "error", "-y"} << args);
    if(!p.waitForStarted(10000))
      return false;
    if(!p.waitForFinished(60000))
      return false;
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
  }

  // rgb24 -> fmt, with the matrix and range ffmpeg is told to write.
  bool encode(
      const QString& fmt, const QString& matrix, const QString& range,
      const QString& out) const
  {
    return run(QStringList{
        "-f", "rawvideo", "-pix_fmt", "rgb24", "-s",
        QString::number(kWidth) + "x" + QString::number(kHeight), "-i", source, "-vf",
        QString("scale=in_range=full:out_range=%1:out_color_matrix=%2")
            .arg(range, matrix),
        "-pix_fmt", fmt, "-f", "rawvideo", out});
  }

  // fmt -> rgb24, ffmpeg reading back what it just wrote. This is the oracle.
  bool decode(
      const QString& fmt, const QString& matrix, const QString& range,
      const QString& in, const QString& out) const
  {
    return run(QStringList{
        "-f", "rawvideo", "-pix_fmt", fmt, "-s",
        QString::number(kWidth) + "x" + QString::number(kHeight), "-i", in, "-vf",
        QString("scale=in_range=%1:in_color_matrix=%2:out_range=full").arg(range, matrix),
        "-pix_fmt", "rgb24", "-f", "rawvideo", out});
  }
};

QByteArray readAll(const QString& path)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  return f.readAll();
}

Fixture makeFixture()
{
  Fixture f;
  f.ffmpeg = ffmpegPath();
  if(!f.ok())
    return f;

  QByteArray src;
  src.reserve(kWidth * kHeight * 3);
  for(int y = 0; y < kHeight; y++)
    for(const auto& c : kColours)
      for(int x = 0; x < kPatch; x++)
        for(int k = 0; k < 3; k++)
          src.push_back(char(c[k]));

  f.source = f.path("src.rgb");
  QFile out{f.source};
  if(!out.open(QIODevice::WriteOnly) || out.write(src) != src.size())
    f.ffmpeg.clear();
  return f;
}

// The interior columns of every patch: everything else can legitimately differ
// between a nearest and an interpolating chroma upsampler.
std::vector<int> interiorColumns()
{
  std::vector<int> cols;
  for(std::size_t p = 0; p < kColours.size(); p++)
    for(int k = 2; k <= 5; k++)
      cols.push_back(int(p) * kPatch + k);
  return cols;
}

struct Rgb
{
  double r{}, g{}, b{};
};

Rgb applyMatrix(const Mat4& m, double y, double u, double v)
{
  return Rgb{
      m[0] * y + m[4] * u + m[8] * v + m[12],
      m[1] * y + m[5] * u + m[9] * v + m[13],
      m[2] * y + m[6] * u + m[10] * v + m[14]};
}

// The shader writes to a UNORM8 target, so the comparable quantity is the
// float times 255 -- comparing rounded integers would hide a half-code scale
// error and trip on ties the two implementations break differently.
double codes(double v)
{
  return std::clamp(v, 0.0, 1.0) * 255.0;
}

uint16_t word(const QByteArray& d, std::size_t i)
{
  return uint16_t(uint8_t(d[2 * i])) | uint16_t(uint8_t(d[2 * i + 1])) << 8;
}

// Largest |predicted - ffmpeg| over the interior of every patch, in 8-bit codes.
// sample(pixelIndex) returns the normalised (Y, Cb, Cr) or (R, G, B) triple the
// shader would see.
template <typename Sample>
double worstError(const QByteArray& oracle, const Mat4& m, Sample&& sample)
{
  REQUIRE(oracle.size() == kWidth * kHeight * 3);
  double worst = 0.;
  for(int y = 0; y < kHeight; y++)
  {
    for(int x : interiorColumns())
    {
      const int i = y * kWidth + x;
      const auto [a, b, c] = sample(i);
      const Rgb got = applyMatrix(m, a, b, c);
      const double want[3]
          = {double(uint8_t(oracle[i * 3])), double(uint8_t(oracle[i * 3 + 1])),
             double(uint8_t(oracle[i * 3 + 2]))};
      worst = std::max(worst, std::abs(codes(got.r) - want[0]));
      worst = std::max(worst, std::abs(codes(got.g) - want[1]));
      worst = std::max(worst, std::abs(codes(got.b) - want[2]));
    }
  }
  return worst;
}

// Half a code is "rounds to the same 8-bit integer as ffmpeg"; the extra
// quarter absorbs the ties the two implementations break in opposite
// directions, plus swscale's own fixed-point error, which reaches 0.51 codes on
// its least precise combination (8-bit limited-range BT.2020). Measured over
// every case below: the shipped constants stay under 0.51, the pre-fix ones
// (chroma centred on 0.5, high-bit-depth normalised by 2^n - 1) miss by 1.11 or
// more. The bound separates them by better than a factor of 1.4 either way.
constexpr double kHalfCode = 0.75;

const Mat4 kIdentity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

struct Space
{
  const char* ffmpegName;
  const char* limited;
  const char* full;
};
const Space kBt709{
    "bt709", SCORE_GFX_BT709_LIMITED_MATRIX, SCORE_GFX_BT709_FULL_MATRIX};
const Space kBt601{
    "bt601", SCORE_GFX_BT601_LIMITED_MATRIX, SCORE_GFX_BT601_FULL_MATRIX};
const Space kBt2020{
    "bt2020ncl", SCORE_GFX_BT2020_LIMITED_MATRIX, SCORE_GFX_BT2020_FULL_MATRIX};

//! The matrix the decoders actually paste into the shader, not a re-derivation
//! of it: the footage half has to fail on a mistyped coefficient too.
Mat4 shipped(const Space& sp, Range r)
{
  return parseMat4(r == Full ? sp.full : sp.limited);
}

// Likewise for the scales -- these are GLSL string literals in the header.
const double kMsbAligned = std::stod(SCORE_GFX_MSB_ALIGNED_SCALE);
const double kLsb10 = std::stod(SCORE_GFX_LSB10_SCALE);
const double kLsb12 = std::stod(SCORE_GFX_LSB12_SCALE);
const double kUnorm10 = std::stod(SCORE_GFX_UNORM10_SCALE);

} // namespace

// ---------------------------------------------------------------------------

TEST_CASE(
    "every colour matrix matches its definition", "[gfx][video][colorspace]")
{
  checkMatrix(
      "BT601 limited", SCORE_GFX_BT601_LIMITED_MATRIX,
      deriveYCbCr(0.299, 0.114, Limited));
  checkMatrix(
      "BT601 full", SCORE_GFX_BT601_FULL_MATRIX, deriveYCbCr(0.299, 0.114, Full));

  checkMatrix(
      "BT709 limited", SCORE_GFX_BT709_LIMITED_MATRIX,
      deriveYCbCr(0.2126, 0.0722, Limited));
  checkMatrix(
      "BT709 full", SCORE_GFX_BT709_FULL_MATRIX, deriveYCbCr(0.2126, 0.0722, Full));

  checkMatrix(
      "SMPTE240M limited", SCORE_GFX_SMPTE240M_LIMITED_MATRIX,
      deriveYCbCr(0.2122, 0.0865, Limited));
  checkMatrix(
      "SMPTE240M full", SCORE_GFX_SMPTE240M_FULL_MATRIX,
      deriveYCbCr(0.2122, 0.0865, Full));

  checkMatrix(
      "FCC limited", SCORE_GFX_FCC_LIMITED_MATRIX, deriveYCbCr(0.30, 0.11, Limited));
  checkMatrix(
      "FCC full", SCORE_GFX_FCC_FULL_MATRIX, deriveYCbCr(0.30, 0.11, Full));

  checkMatrix(
      "BT2020 limited", SCORE_GFX_BT2020_LIMITED_MATRIX,
      deriveYCbCr(0.2627, 0.0593, Limited));
  checkMatrix(
      "BT2020 full", SCORE_GFX_BT2020_FULL_MATRIX,
      deriveYCbCr(0.2627, 0.0593, Full));

  checkMatrix("YCgCo limited", SCORE_GFX_YCGCO_LIMITED_MATRIX, deriveYCgCo(Limited));
  checkMatrix("YCgCo full", SCORE_GFX_YCGCO_FULL_MATRIX, deriveYCgCo(Full));

  checkMatrix("RGB identity", SCORE_GFX_RGB_MATRIX, kIdentity);
}

TEST_CASE(
    "the aliases still point at the limited-range matrices",
    "[gfx][video][colorspace]")
{
  // Callers that pass no range get limited, which is what an untagged stream is.
  CHECK(
      std::string_view{SCORE_GFX_BT601_MATRIX}
      == std::string_view{SCORE_GFX_BT601_LIMITED_MATRIX});
  CHECK(
      std::string_view{SCORE_GFX_BT709_MATRIX}
      == std::string_view{SCORE_GFX_BT709_LIMITED_MATRIX});
}

TEST_CASE(
    "the normalisation constants are the ratios they claim to be",
    "[gfx][video][colorspace]")
{
  // Each is spelled as a GLSL literal, so a typo is invisible to the compiler.
  CHECK(std::stod(SCORE_GFX_MSB_ALIGNED_SCALE) == 65535.0 / 65280.0);
  CHECK(std::stod(SCORE_GFX_LSB10_SCALE) == 65535.0 / 1020.0);
  CHECK(std::stod(SCORE_GFX_LSB12_SCALE) == 65535.0 / 4080.0);
  CHECK(std::abs(std::stod(SCORE_GFX_UNORM10_SCALE) - 1023.0 / 1020.0) < 1e-15);

  // 65280 is 255 << 8 and 1020 is 255 << 2: every one of these divides by the
  // 8-bit full scale shifted up, never by 2^n - 1. That identity is what lets a
  // single 8-bit-normalised matrix serve every depth.
  CHECK(65280 == 255 << 8);
  CHECK(1020 == 255 << 2);
  CHECK(4080 == 255 << 4);
}

// ---------------------------------------------------------------------------
//  Footage
// ---------------------------------------------------------------------------

TEST_CASE(
    "planar 4:4:4 decodes as ffmpeg does at every depth and range",
    "[gfx][video][colorspace][footage]")
{
  const Fixture fx = makeFixture();
  if(!fx.ok())
    SKIP("ffmpeg not available");

  // 4:4:4 keeps chroma per-pixel, so this isolates the matrix and the scale
  // from every resampling question. Full range is the case the 128/255 centre
  // fix is about; limited range checks the matrices that were already right.
  struct Case
  {
    const char* fmt;
    int depth;
    double scale; // maps the sampled UNORM16 value onto the 8-bit scale
  };
  const Case cases[]{
      {"yuv444p", 8, 1.0},
      {"yuv444p10le", 10, kLsb10},
      {"yuv444p12le", 12, kLsb12},
      {"yuv444p16le", 16, kMsbAligned},
  };

  for(const Space& sp : {kBt709, kBt601, kBt2020})
  {
    for(const char* range : {"full", "tv"})
    {
      for(const Case& c : cases)
      {
        INFO(c.fmt << " " << sp.ffmpegName << " " << range);
        const QString raw = fx.path(QString("%1.raw").arg(c.fmt));
        const QString ref = fx.path(QString("%1.ref").arg(c.fmt));
        if(!fx.encode(c.fmt, sp.ffmpegName, range, raw))
        {
          WARN("not validated, this ffmpeg cannot write it: " << c.fmt);
          continue;
        }
        REQUIRE(fx.decode(c.fmt, sp.ffmpegName, range, raw, ref));

        const QByteArray d = readAll(raw);
        const QByteArray oracle = readAll(ref);
        const int n = kWidth * kHeight;
        REQUIRE(d.size() == (c.depth == 8 ? 3 * n : 6 * n));

        const Mat4 m = shipped(sp, std::string_view{range} == "full" ? Full : Limited);
        const double err = worstError(oracle, m, [&](int i) {
          auto at = [&](int k) {
            return c.depth == 8 ? double(uint8_t(d[k])) / 255.
                                : word(d, std::size_t(k)) * c.scale / 65535.;
          };
          return std::array<double, 3>{at(i), at(n + i), at(2 * n + i)};
        });
        CHECK(err <= kHalfCode);
      }
    }
  }
}

TEST_CASE(
    "packed 4:4:4 decodes as ffmpeg does", "[gfx][video][colorspace][footage]")
{
  const Fixture fx = makeFixture();
  if(!fx.ok())
    SKIP("ffmpeg not available");

  const int n = kWidth * kHeight;

  SECTION("vuya: V, U, Y, A in memory order")
  {
    for(const Space& sp : {kBt709, kBt601})
    {
      for(const char* range : {"full", "tv"})
      {
        INFO("vuya " << sp.ffmpegName << " " << range);
        const QString raw = fx.path("vuya.raw"), ref = fx.path("vuya.ref");
        if(!fx.encode("vuya", sp.ffmpegName, range, raw))
        {
          WARN("not validated, this ffmpeg cannot write it: vuya");
          continue;
        }
        REQUIRE(fx.decode("vuya", sp.ffmpegName, range, raw, ref));

        const QByteArray d = readAll(raw);
        REQUIRE(d.size() == 4 * n);
        const Mat4 m = shipped(sp, std::string_view{range} == "full" ? Full : Limited);
        // The decoder samples an RGBA8 texture, so tex.r is the first byte:
        // Y is tex.b, Cb is tex.g, Cr is tex.r -- the "bgr" swizzle.
        const double err = worstError(readAll(ref), m, [&](int i) {
          return std::array<double, 3>{
              uint8_t(d[i * 4 + 2]) / 255., uint8_t(d[i * 4 + 1]) / 255.,
              uint8_t(d[i * 4 + 0]) / 255.};
        });
        CHECK(err <= kHalfCode);
      }
    }
  }

  SECTION("xv30: three 10-bit fields in a 32-bit word, hardware-normalised")
  {
    for(const char* range : {"full", "tv"})
    {
      INFO("xv30le " << range);
      const QString raw = fx.path("xv30.raw"), ref = fx.path("xv30.ref");
      if(!fx.encode("xv30le", kBt709.ffmpegName, range, raw))
      {
        WARN("not validated, this ffmpeg cannot write it: xv30le");
        continue;
      }
      REQUIRE(fx.decode("xv30le", kBt709.ffmpegName, range, raw, ref));

      const QByteArray d = readAll(raw);
      REQUIRE(d.size() == 4 * n);
      const Mat4 m
          = shipped(kBt709, std::string_view{range} == "full" ? Full : Limited);
      // RGB10A2 hands the shader code/1023; the matrix wants code/1020, which
      // is what SCORE_GFX_UNORM10_SCALE corrects.
      const double s = kUnorm10;
      const double err = worstError(readAll(ref), m, [&](int i) {
        const uint32_t w = uint32_t(uint8_t(d[i * 4])) | uint32_t(uint8_t(d[i * 4 + 1])) << 8
                           | uint32_t(uint8_t(d[i * 4 + 2])) << 16
                           | uint32_t(uint8_t(d[i * 4 + 3])) << 24;
        return std::array<double, 3>{
            ((w >> 10) & 0x3FF) / 1023. * s, (w & 0x3FF) / 1023. * s,
            ((w >> 20) & 0x3FF) / 1023. * s};
      });
      CHECK(err <= kHalfCode);
    }
  }

  SECTION("p410: 10 bits left-aligned in a 16-bit word, 4:4:4")
  {
    for(const Space& sp : {kBt709, kBt601, kBt2020})
    {
      INFO("p410le " << sp.ffmpegName);
      const QString raw = fx.path("p410.raw"), ref = fx.path("p410.ref");
      if(!fx.encode("p410le", sp.ffmpegName, "tv", raw))
      {
        WARN("not validated, this ffmpeg cannot write it: p410le");
        continue;
      }
      REQUIRE(fx.decode("p410le", sp.ffmpegName, "tv", raw, ref));

      const QByteArray d = readAll(raw);
      REQUIRE(d.size() == 6 * n);
      const Mat4 m = shipped(sp, Limited);
      const double s = kMsbAligned;
      const double err = worstError(readAll(ref), m, [&](int i) {
        return std::array<double, 3>{
            word(d, std::size_t(i)) * s / 65535.,
            word(d, std::size_t(n + 2 * i)) * s / 65535.,
            word(d, std::size_t(n + 2 * i + 1)) * s / 65535.};
      });
      CHECK(err <= kHalfCode);
    }
  }
}

TEST_CASE(
    "high-bit-depth RGB decodes as ffmpeg does", "[gfx][video][colorspace][footage]")
{
  const Fixture fx = makeFixture();
  if(!fx.ok())
    SKIP("ffmpeg not available");

  const int n = kWidth * kHeight;

  SECTION("planar GBR(A)")
  {
    struct Case
    {
      const char* fmt;
      double scale;
    };
    const Case cases[]{
        {"gbrp10le", kLsb10},  {"gbrp12le", kLsb12},  {"gbrp16le", kMsbAligned},
        {"gbrap10le", kLsb10}, {"gbrap12le", kLsb12}, {"gbrap16le", kMsbAligned},
    };
    for(const Case& c : cases)
    {
      INFO(c.fmt);
      const QString raw = fx.path(QString("%1.raw").arg(c.fmt));
      const QString ref = fx.path(QString("%1.ref").arg(c.fmt));
      if(!fx.encode(c.fmt, "bt709", "full", raw))
      {
        WARN("not validated, this ffmpeg cannot write it: " << c.fmt);
        continue;
      }
      REQUIRE(fx.decode(c.fmt, "bt709", "full", raw, ref));

      const QByteArray d = readAll(raw);
      const double err = worstError(readAll(ref), kIdentity, [&](int i) {
        // Plane order is G, B, R.
        return std::array<double, 3>{
            word(d, std::size_t(2 * n + i)) * c.scale / 65535.,
            word(d, std::size_t(i)) * c.scale / 65535.,
            word(d, std::size_t(n + i)) * c.scale / 65535.};
      });
      CHECK(err <= kHalfCode);
    }
  }

  SECTION("packed 16 bits per channel")
  {
    // RGB48Decoder and RGBA64Decoder both read these through an R16 data
    // texture and scale by SCORE_GFX_MSB_ALIGNED_SCALE.
    struct Case
    {
      const char* fmt;
      int channels;
      bool bgr;
    };
    const Case cases[]{
        {"rgb48le", 3, false},
        {"bgr48le", 3, true},
        {"rgba64le", 4, false},
        {"bgra64le", 4, true},
    };
    const double s = kMsbAligned;
    for(const Case& c : cases)
    {
      INFO(c.fmt);
      const QString raw = fx.path(QString("%1.raw").arg(c.fmt));
      const QString ref = fx.path(QString("%1.ref").arg(c.fmt));
      if(!fx.encode(c.fmt, "bt709", "full", raw))
      {
        WARN("not validated, this ffmpeg cannot write it: " << c.fmt);
        continue;
      }
      REQUIRE(fx.decode(c.fmt, "bt709", "full", raw, ref));

      const QByteArray d = readAll(raw);
      REQUIRE(d.size() == 2 * c.channels * n);
      const double err = worstError(readAll(ref), kIdentity, [&](int i) {
        const int base = i * c.channels;
        std::array<double, 3> v{
            word(d, std::size_t(base)) * s / 65535.,
            word(d, std::size_t(base + 1)) * s / 65535.,
            word(d, std::size_t(base + 2)) * s / 65535.};
        if(c.bgr)
          std::swap(v[0], v[2]);
        return v;
      });
      CHECK(err <= kHalfCode);
    }
  }
}

TEST_CASE(
    "MSB-aligned formats are the planar codes shifted up, on real footage",
    "[gfx][video][colorspace][footage]")
{
  const Fixture fx = makeFixture();
  if(!fx.ok())
    SKIP("ffmpeg not available");

  // The claim SCORE_GFX_MSB_ALIGNED_SCALE rests on: a P-family word is the
  // n-bit code left-aligned in 16 bits, so dividing it by 65280 is the same as
  // dividing the n-bit code by 255 << (n - 8). Checked against ffmpeg's own
  // planar rendering of the same source rather than asserted.
  //
  // This is checked separately from a decode comparison because swscale's
  // *input* path for subsampled high-bit-depth YUV is itself imprecise: it
  // renders limited-range white as 253 where 8-bit 4:2:0, 10-bit 4:4:4 and
  // zimg all give 255. The layout identity is exact; the round-trip through
  // that path is not, and pinning it would pin swscale's error.
  struct Pair
  {
    const char* packed;
    const char* planar;
    int shift;
    int chromaW, chromaH;
  };
  const Pair pairs[]{
      {"p010le", "yuv420p10le", 6, 2, 2},
      {"p016le", "yuv420p16le", 0, 2, 2},
      {"p210le", "yuv422p10le", 6, 2, 1},
      {"p410le", "yuv444p10le", 6, 1, 1},
  };

  for(const Pair& p : pairs)
  {
    INFO(p.packed << " vs " << p.planar);
    const QString a = fx.path(QString("%1.a").arg(p.packed));
    const QString b = fx.path(QString("%1.b").arg(p.packed));
    if(!fx.encode(p.packed, "bt709", "tv", a)
       || !fx.encode(p.planar, "bt709", "tv", b))
    {
      WARN("not validated, this ffmpeg cannot write it: " << p.packed);
      continue;
    }

    const QByteArray A = readAll(a), B = readAll(b);
    const int n = kWidth * kHeight;
    const int cn = (kWidth / p.chromaW) * (kHeight / p.chromaH);
    REQUIRE(A.size() == 2 * (n + 2 * cn));
    REQUIRE(B.size() == 2 * (n + 2 * cn));

    for(int i = 0; i < n; i++)
    {
      INFO("luma " << i);
      REQUIRE(word(A, std::size_t(i)) == uint16_t(word(B, std::size_t(i)) << p.shift));
    }
    for(int i = 0; i < cn; i++)
    {
      INFO("chroma " << i);
      // Semiplanar interleaves Cb and Cr; planar keeps two planes.
      REQUIRE(
          word(A, std::size_t(n + 2 * i))
          == uint16_t(word(B, std::size_t(n + i)) << p.shift));
      REQUIRE(
          word(A, std::size_t(n + 2 * i + 1))
          == uint16_t(word(B, std::size_t(n + cn + i)) << p.shift));
    }
  }
}

TEST_CASE(
    "the Y210 texel mapping reaches the right macropixel samples",
    "[gfx][video][colorspace][footage]")
{
  const Fixture fx = makeFixture();
  if(!fx.ok())
    SKIP("ffmpeg not available");

  const QString raw = fx.path("y210.raw"), planar = fx.path("y210.planar");
  if(!fx.encode("y210le", "bt709", "tv", raw))
    SKIP("this ffmpeg cannot write y210le");
  REQUIRE(fx.encode("yuv422p10le", "bt709", "tv", planar));

  const QByteArray d = readAll(raw), P = readAll(planar);
  const int n = kWidth * kHeight;
  const int cn = (kWidth / 2) * kHeight;
  REQUIRE(d.size() == 4 * n); // Y0 Cb Y1 Cr per macropixel, 4 bytes per pixel

  // Y210Decoder uploads the row into an RG16 texture w texels wide, so texel x
  // is (word 2x, word 2x + 1) of the row: luma of pixel x in .r, and in .g the
  // Cb of the macropixel on even x, its Cr on odd x. That is what the shader's
  // cx = (x / 2) * 2 indexing assumes.
  for(int y = 0; y < kHeight; y++)
  {
    for(int x = 0; x < kWidth; x++)
    {
      const std::size_t base = std::size_t(y) * kWidth * 2;
      const int cx = (x / 2) * 2;
      const uint16_t luma = word(d, base + std::size_t(x) * 2);
      const uint16_t cb = word(d, base + std::size_t(cx) * 2 + 1);
      const uint16_t cr = word(d, base + std::size_t(cx + 1) * 2 + 1);

      INFO("pixel " << x << "," << y);
      REQUIRE(luma == uint16_t(word(P, std::size_t(y) * kWidth + x) << 6));
      const std::size_t ci = std::size_t(n) + std::size_t(y) * (kWidth / 2) + x / 2;
      REQUIRE(cb == uint16_t(word(P, ci) << 6));
      REQUIRE(cr == uint16_t(word(P, ci + std::size_t(cn)) << 6));
    }
  }
}

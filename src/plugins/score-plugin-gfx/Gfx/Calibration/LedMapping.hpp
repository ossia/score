#pragma once
#include <score_plugin_gfx_export.h>
#include <Eigen/Geometry>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// LED surfaces and LED fixtures:
//  - XYZ maps of LED walls, flat (four corners) or made of cabinets placed in
//    the output raster, as the mesh / MPCDI geometry nDisplay and media servers
//    use for LED volumes;
//  - finding LEDs with a camera: every LED shows its index in binary, one bit
//    per frame followed by its inverse, so N LEDs need 2 + 2 ceil(log2(N + 1))
//    frames instead of the N of one-at-a-time scanning (MariMapper); the
//    binary mode of Lightwork (github.com/PWRFLcreative/Lightwork) and the
//    "unique subset of light elements per test frame" of US 9,942,970 B2 are
//    the same idea. Pairs of frames with the inverse make the bit decision
//    independent of each LED's brightness, as in Gray-code structured light.
namespace score::gfx::led
{
using Vec3 = Eigen::Vector3d;

//! Largest raster side and pixel count rasterize() accepts.
inline constexpr int maxRasterSide = 16384;
inline constexpr long long maxRasterPixels = 8192LL * 8192;

struct Module
{
  //! Origin and size in the output raster, pixels.
  int x{}, y{}, w{}, h{};
  //! World corners: top-left, top-right, bottom-left, bottom-right.
  Vec3 tl, tr, bl, br;
};

//! One cabinet per line: x, y, w, h, then the TL, TR, BL, BR corners
//! (12 numbers); blank lines, lines starting with '#' and a non-numeric header
//! are skipped.
SCORE_PLUGIN_GFX_EXPORT std::vector<Module> parseModules(std::string_view csv, std::string* error = nullptr);

//! The world point of each LED pixel: bilinear over the module corners at the
//! pixel centres. RGBA, rows top to bottom, w = 1 on module pixels, 0 in gaps.
//! Empty when W x H is beyond maxRasterSide / maxRasterPixels.
inline std::vector<float>
rasterize(const std::vector<Module>& modules, int W, int H)
{
  if(W <= 0 || H <= 0 || W > maxRasterSide || H > maxRasterSide
     || (long long)W * H > maxRasterPixels)
    return {};
  std::vector<float> xyz(std::size_t(W) * H * 4, 0.f);
  for(const auto& m : modules)
  {
    const long long j0 = std::max(0LL, -(long long)m.y);
    const long long j1 = std::min<long long>(m.h, (long long)H - m.y);
    const long long i0 = std::max(0LL, -(long long)m.x);
    const long long i1 = std::min<long long>(m.w, (long long)W - m.x);
    for(long long j = j0; j < j1; j++)
    {
      const int y = int(m.y + j);
      const double t = (j + 0.5) / m.h;
      const Vec3 left = m.tl + t * (m.bl - m.tl), right = m.tr + t * (m.br - m.tr);
      for(long long i = i0; i < i1; i++)
      {
        const int x = int(m.x + i);
        const double s = (i + 0.5) / m.w;
        const Vec3 p = left + s * (right - left);
        float* q = xyz.data() + (std::size_t(y) * W + x) * 4;
        q[0] = float(p.x());
        q[1] = float(p.y());
        q[2] = float(p.z());
        q[3] = 1.f;
      }
    }
  }
  return xyz;
}

//! The bounding raster of the modules, at most maxRasterSide on each side.
inline std::array<int, 2> rasterSize(const std::vector<Module>& modules)
{
  long long w = 0, h = 0;
  for(const auto& m : modules)
  {
    w = std::max(w, (long long)m.x + m.w);
    h = std::max(h, (long long)m.y + m.h);
  }
  return {int(std::min<long long>(w, maxRasterSide)), int(std::min<long long>(h, maxRasterSide))};
}

struct Screen
{
  //! Kooima corners: lower-left, lower-right, upper-left.
  Vec3 pa, pb, pc;
};

//! The screen of one module: its outer pixel edges.
inline Screen moduleScreen(const Module& m)
{
  return {m.bl, m.br, m.tl};
}

//! A rectangle in the least-squares plane of all corners, along the first
//! module's rows, bounding every corner.
SCORE_PLUGIN_GFX_EXPORT Screen boundingScreen(const std::vector<Module>& modules);

//! Binary blink code.
struct BlinkCode
{
  int count{};
  int bits{};

  //! Count clamped to [1, 2^24].
  static BlinkCode make(int count) noexcept
  {
    BlinkCode c;
    c.count = std::clamp(count, 1, 1 << 24);
    c.bits = 1;
    while((1 << c.bits) < c.count + 1)
      c.bits++;
    return c;
  }
  //! Frame 0: all on, 1: all off, then bit b (MSB first) and its inverse.
  int frames() const noexcept { return 2 + 2 * bits; }
  //! Codes start at 1: an LED never shows all zeros, so a pixel lit by no LED
  //! cannot pass for LED 0.
  uint32_t code(int index) const noexcept { return uint32_t(index + 1); }
  bool on(int frame, int index) const noexcept
  {
    if(frame == 0)
      return true;
    if(frame == 1)
      return false;
    const int b = (frame - 2) / 2;
    const bool bit = (code(index) >> (bits - 1 - b)) & 1u;
    return (frame % 2 == 0) ? bit : !bit;
  }
};

struct Found
{
  int index{};
  //! Camera pixel centroid (pixel centres at integers) and normalized (v = 0 top).
  double u{}, v{};
  double nu{}, nv{};
  //! Mean world point of the pixels, when positions were given.
  Vec3 world{Vec3::Zero()};
  bool hasWorld{};
  int pixels{};
  double confidence{};
};

struct DecodeOptions
{
  //! (on - off) above this, in camera units.
  double minSignal{0.05};
  //! |I - Iinv| / (on - off) above this for every bit.
  double minContrast{0.25};
  int minPixels{1};
};

//! frames: the captures in BlinkCode order, luminance per camera pixel.
//! positions: optional RGBA world points per camera pixel (w > 0 valid).
SCORE_PLUGIN_GFX_EXPORT std::vector<Found> decode(
    const BlinkCode& code, const std::vector<std::vector<float>>& frames, int W, int H,
    const DecodeOptions& opt, const std::vector<float>* positions = nullptr);
}

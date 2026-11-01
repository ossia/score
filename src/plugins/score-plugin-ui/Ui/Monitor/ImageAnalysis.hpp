#pragma once
#include <algorithm>
#include <array>
#include <cmath>

/**
 * What the image scopes' graticules are drawn from: luma and colour
 * difference per ITU-R BT.601, BT.709 and BT.2020, bar targets, the PQ
 * signal of a luminance, primaries and the CIE 1931 loci. The per-pixel
 * transfer functions are the GPU ones of Gfx/Graph/decoders/ColorSpace.hpp.
 */
namespace Ui::Monitor::Color
{
enum class Matrix
{
  Rec601,
  Rec709,
  Rec2020
};

struct LumaCoefficients
{
  double kr, kg, kb;
};

constexpr LumaCoefficients coefficients(Matrix m) noexcept
{
  switch(m)
  {
    case Matrix::Rec601:
      return {0.299, 0.587, 0.114};
    case Matrix::Rec2020:
      return {0.2627, 0.6780, 0.0593};
    default:
      return {0.2126, 0.7152, 0.0722};
  }
}

struct YCbCr
{
  double y, cb, cr;
};

//! Y' in [0, 1], Cb and Cr in [-0.5, 0.5] for R'G'B' in [0, 1].
constexpr YCbCr toYCbCr(double r, double g, double b, Matrix m) noexcept
{
  const auto k = coefficients(m);
  const double y = k.kr * r + k.kg * g + k.kb * b;
  return {y, (b - y) / (2. * (1. - k.kb)), (r - y) / (2. * (1. - k.kr))};
}

//! The six colour bars at full amplitude, R, Yl, G, Cy, B, Mg: where their
//! targets sit on a vectorscope for the given matrix.
inline std::array<YCbCr, 6> barTargets(Matrix m, double amplitude = 1.) noexcept
{
  constexpr double bars[6][3]{{1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0, 1, 1}, {0, 0, 1}, {1, 0, 1}};
  std::array<YCbCr, 6> out{};
  for(int i = 0; i < 6; i++)
    out[i] = toYCbCr(
        bars[i][0] * amplitude, bars[i][1] * amplitude, bars[i][2] * amplitude, m);
  return out;
}

inline constexpr const char* bar_names[6]{"R", "Yl", "G", "Cy", "B", "Mg"};

//! The skin tone line of NTSC vectorscopes, the I axis: 123 degrees
//! counter-clockwise from +Cb.
inline constexpr double skin_tone_degrees = 123.;

enum class Transfer
{
  Sdr,
  PQ,
  HLG
};

//! The PQ signal value of a luminance in cd/m2.
inline double pqOfNits(double nits) noexcept
{
  constexpr double m1 = 0.1593017578125, m2 = 78.84375;
  constexpr double c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
  const double y = std::pow(std::clamp(nits / 10000., 0., 1.), m1);
  return std::pow((c1 + c2 * y) / (1. + c3 * y), m2);
}

struct Primaries
{
  double rx, ry, gx, gy, bx, by, wx, wy;
};

inline constexpr Primaries rec709{0.640, 0.330, 0.300, 0.600, 0.150, 0.060, 0.3127, 0.3290};
inline constexpr Primaries dciP3{0.680, 0.320, 0.265, 0.690, 0.150, 0.060, 0.3127, 0.3290};
inline constexpr Primaries rec2020{0.708, 0.292, 0.170, 0.797, 0.131, 0.046, 0.3127, 0.3290};

inline const Primaries& primariesOf(Matrix m) noexcept
{
  return m == Matrix::Rec2020 ? rec2020 : rec709;
}

using Mat3 = std::array<std::array<double, 3>, 3>;

//! Linear RGB to CIE XYZ for a set of primaries and white point.
inline Mat3 rgbToXyz(const Primaries& p) noexcept
{
  auto xyz = [](double x, double y) { return std::array<double, 3>{x / y, 1., (1. - x - y) / y}; };
  const auto r = xyz(p.rx, p.ry), g = xyz(p.gx, p.gy), b = xyz(p.bx, p.by), w = xyz(p.wx, p.wy);
  const Mat3 m{{{r[0], g[0], b[0]}, {r[1], g[1], b[1]}, {r[2], g[2], b[2]}}};
  // Scale each primary so that they sum to the white point
  const double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
                     - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                     + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
  Mat3 inv{};
  inv[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) / det;
  inv[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) / det;
  inv[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) / det;
  inv[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) / det;
  inv[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) / det;
  inv[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) / det;
  inv[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) / det;
  inv[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) / det;
  inv[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) / det;
  std::array<double, 3> s{};
  for(int i = 0; i < 3; i++)
    s[i] = inv[i][0] * w[0] + inv[i][1] * w[1] + inv[i][2] * w[2];
  Mat3 out{};
  for(int i = 0; i < 3; i++)
    for(int j = 0; j < 3; j++)
      out[i][j] = m[i][j] * s[j];
  return out;
}

//! CIE 1931 2-degree spectral locus, 380 to 700 nm.
inline constexpr std::array<std::array<double, 2>, 25> spectral_locus{{
    {0.1741, 0.0050}, {0.1714, 0.0051}, {0.1689, 0.0069}, {0.1644, 0.0109},
    {0.1566, 0.0177}, {0.1440, 0.0297}, {0.1241, 0.0578}, {0.0913, 0.1327},
    {0.0454, 0.2950}, {0.0082, 0.5384}, {0.0139, 0.7502}, {0.0743, 0.8338},
    {0.1547, 0.8059}, {0.2296, 0.7543}, {0.3016, 0.6923}, {0.3731, 0.6245},
    {0.4441, 0.5547}, {0.5125, 0.4866}, {0.5752, 0.4242}, {0.6270, 0.3725},
    {0.6658, 0.3340}, {0.6915, 0.3083}, {0.7079, 0.2920}, {0.7190, 0.2809},
    {0.7347, 0.2653}}};

//! Chromaticity of a black body at T kelvin, 1667 to 25000 K (Kim et al.).
inline std::array<double, 2> planckian(double t) noexcept
{
  t = std::clamp(t, 1667., 25000.);
  const double t2 = t * t, t3 = t2 * t;
  const double x = t <= 4000.
                       ? -0.2661239e9 / t3 - 0.2343589e6 / t2 + 0.8776956e3 / t + 0.179910
                       : -3.0258469e9 / t3 + 2.1070379e6 / t2 + 0.2226347e3 / t + 0.240390;
  const double x2 = x * x, x3 = x2 * x;
  double y;
  if(t <= 2222.)
    y = -1.1063814 * x3 - 1.34811020 * x2 + 2.18555832 * x - 0.20219683;
  else if(t <= 4000.)
    y = -0.9549476 * x3 - 1.37418593 * x2 + 2.09137015 * x - 0.16748867;
  else
    y = 3.0817580 * x3 - 5.87338670 * x2 + 3.75112997 * x - 0.37001483;
  return {x, y};
}
}

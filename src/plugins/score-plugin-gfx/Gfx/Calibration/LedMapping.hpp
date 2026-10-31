#pragma once
#include <Eigen/Dense>

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
inline std::vector<Module> parseModules(std::string_view csv, std::string* error = nullptr)
{
  std::vector<Module> out;
  std::size_t pos = 0;
  int line = 0;
  while(pos <= csv.size())
  {
    std::size_t end = csv.find('\n', pos);
    if(end == std::string_view::npos)
      end = csv.size();
    std::string l(csv.substr(pos, end - pos));
    pos = end + 1;
    line++;
    for(char& c : l)
      if(c == ',' || c == ';' || c == '\t' || c == '\r')
        c = ' ';
    std::size_t b = l.find_first_not_of(' ');
    if(b == std::string::npos || l[b] == '#')
      continue;
    std::istringstream s(l);
    std::vector<double> v;
    double d;
    while(s >> d)
      v.push_back(d);
    if(v.empty())
      continue;
    if(v.size() < 16)
    {
      if(error)
        *error = "line " + std::to_string(line) + ": " + std::to_string(v.size())
                 + " numbers, 16 expected (x y w h, then TL TR BL BR xyz)";
      continue;
    }
    Module m;
    m.x = int(v[0]);
    m.y = int(v[1]);
    m.w = int(v[2]);
    m.h = int(v[3]);
    m.tl = Vec3(v[4], v[5], v[6]);
    m.tr = Vec3(v[7], v[8], v[9]);
    m.bl = Vec3(v[10], v[11], v[12]);
    m.br = Vec3(v[13], v[14], v[15]);
    if(m.w > 0 && m.h > 0)
      out.push_back(m);
  }
  return out;
}

//! The world point of each LED pixel: bilinear over the module corners at the
//! pixel centres. RGBA, rows top to bottom, w = 1 on module pixels, 0 in gaps.
inline std::vector<float>
rasterize(const std::vector<Module>& modules, int W, int H)
{
  std::vector<float> xyz(std::size_t(W) * H * 4, 0.f);
  for(const auto& m : modules)
    for(int j = 0; j < m.h; j++)
    {
      const int y = m.y + j;
      if(y < 0 || y >= H)
        continue;
      const double t = (j + 0.5) / m.h;
      const Vec3 left = m.tl + t * (m.bl - m.tl), right = m.tr + t * (m.br - m.tr);
      for(int i = 0; i < m.w; i++)
      {
        const int x = m.x + i;
        if(x < 0 || x >= W)
          continue;
        const double s = (i + 0.5) / m.w;
        const Vec3 p = left + s * (right - left);
        float* q = xyz.data() + (std::size_t(y) * W + x) * 4;
        q[0] = float(p.x());
        q[1] = float(p.y());
        q[2] = float(p.z());
        q[3] = 1.f;
      }
    }
  return xyz;
}

//! The bounding raster of the modules.
inline std::array<int, 2> rasterSize(const std::vector<Module>& modules)
{
  int w = 0, h = 0;
  for(const auto& m : modules)
  {
    w = std::max(w, m.x + m.w);
    h = std::max(h, m.y + m.h);
  }
  return {w, h};
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
inline Screen boundingScreen(const std::vector<Module>& modules)
{
  Screen s;
  if(modules.empty())
    return s;
  std::vector<Vec3> pts;
  for(const auto& m : modules)
    for(const Vec3& p : {m.tl, m.tr, m.bl, m.br})
      pts.push_back(p);
  Vec3 c = Vec3::Zero();
  for(auto& p : pts)
    c += p;
  c /= double(pts.size());
  Eigen::Matrix3d C = Eigen::Matrix3d::Zero();
  for(auto& p : pts)
    C += (p - c) * (p - c).transpose();
  Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> es(C);
  const Vec3 n = es.eigenvectors().col(0);
  const auto& m0 = modules.front();
  Vec3 vr = (m0.tr - m0.tl) + (m0.br - m0.bl);
  vr = (vr - vr.dot(n) * n).normalized();
  Vec3 vu = n.cross(vr);
  if(vu.dot(m0.tl - m0.bl) < 0.)
    vu = -vu;
  double x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300;
  for(auto& p : pts)
  {
    x0 = std::min(x0, vr.dot(p - c));
    x1 = std::max(x1, vr.dot(p - c));
    y0 = std::min(y0, vu.dot(p - c));
    y1 = std::max(y1, vu.dot(p - c));
  }
  s.pa = c + x0 * vr + y0 * vu;
  s.pb = c + x1 * vr + y0 * vu;
  s.pc = c + x0 * vr + y1 * vu;
  return s;
}

//! Binary blink code.
struct BlinkCode
{
  int count{};
  int bits{};

  static BlinkCode make(int count) noexcept
  {
    BlinkCode c;
    c.count = std::max(1, count);
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
inline std::vector<Found> decode(
    const BlinkCode& code, const std::vector<std::vector<float>>& frames, int W, int H,
    const DecodeOptions& opt, const std::vector<float>* positions = nullptr)
{
  std::vector<Found> out;
  if(int(frames.size()) < code.frames())
    return out;
  const std::size_t n = std::size_t(W) * H;
  struct Acc
  {
    double w{}, u{}, v{}, c{};
    Vec3 p{Vec3::Zero()};
    double pw{};
    int count{};
  };
  std::vector<Acc> acc(code.count);
  for(std::size_t i = 0; i < n; i++)
  {
    const double on = frames[0][i], off = frames[1][i];
    const double span = on - off;
    if(span < opt.minSignal)
      continue;
    uint32_t c = 0;
    double minC = 1e9;
    bool ok = true;
    for(int b = 0; b < code.bits && ok; b++)
    {
      const double a = frames[2 + 2 * b][i], ainv = frames[3 + 2 * b][i];
      const double k = std::abs(a - ainv) / span;
      ok = k >= opt.minContrast;
      minC = std::min(minC, k);
      c = (c << 1) | (a > ainv ? 1u : 0u);
    }
    if(!ok || c == 0 || int(c) > code.count)
      continue;
    auto& A = acc[c - 1];
    A.w += span;
    A.u += span * double(i % W);
    A.v += span * double(i / W);
    A.c += span * minC;
    A.count++;
    if(positions)
    {
      const float* p = positions->data() + i * 4;
      if(p[3] > 0.f && std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2]))
      {
        A.p += span * Vec3(p[0], p[1], p[2]);
        A.pw += span;
      }
    }
  }
  for(int k = 0; k < code.count; k++)
  {
    const auto& A = acc[k];
    if(A.count < opt.minPixels || A.w <= 0.)
      continue;
    Found f;
    f.index = k;
    f.u = A.u / A.w;
    f.v = A.v / A.w;
    f.nu = (f.u + 0.5) / W;
    f.nv = (f.v + 0.5) / H;
    f.pixels = A.count;
    f.confidence = A.c / A.w;
    if(A.pw > 0.)
    {
      f.world = A.p / A.pw;
      f.hasWorld = true;
    }
    out.push_back(f);
  }
  return out;
}
}

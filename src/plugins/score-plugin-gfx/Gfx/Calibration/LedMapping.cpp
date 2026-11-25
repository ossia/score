#include <Gfx/Calibration/LedMapping.hpp>
#include <Eigen/Dense>

namespace score::gfx::led
{

std::vector<Module> parseModules(std::string_view csv, std::string* error)
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
    const bool inRange = std::all_of(v.begin(), v.begin() + 4, [](double d) {
      return std::abs(d) <= 65536.;
    }) && std::all_of(v.begin() + 4, v.begin() + 16, [](double d) { return std::isfinite(d); });
    if(!inRange)
    {
      if(error)
        *error = "line " + std::to_string(line)
                 + ": raster coordinates beyond 65536 or non-finite corners";
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

Screen boundingScreen(const std::vector<Module>& modules)
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

std::vector<Found> decode(
    const BlinkCode& code, const std::vector<std::vector<float>>& frames, int W, int H,
    const DecodeOptions& opt, const std::vector<float>* positions)
{
  std::vector<Found> out;
  if(W <= 0 || H <= 0 || code.count < 1 || int(frames.size()) < code.frames())
    return out;
  const std::size_t n = std::size_t(W) * H;
  for(int k = 0; k < code.frames(); k++)
    if(frames[k].size() < n)
      return out;
  if(positions && positions->size() < n * 4)
    positions = nullptr;
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

#include "OutputQuality.hpp"

#include <QFile>
#include <QList>

#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <random>

namespace score::gfx
{
namespace
{
bool parseFloats(const QList<QByteArray>& tok, int first, float* out, int n)
{
  if(tok.size() < first + n)
    return false;
  for(int i = 0; i < n; i++)
  {
    bool ok{};
    out[i] = tok[first + i].toFloat(&ok);
    if(!ok)
      return false;
  }
  return true;
}

float sample1D(const std::vector<float>& t, int n, int channel, float x)
{
  const float f = std::clamp(x, 0.f, 1.f) * float(n - 1);
  const int i0 = std::min(int(f), n - 1);
  const int i1 = std::min(i0 + 1, n - 1);
  const float a = f - float(i0);
  return t[i0 * 3 + channel] * (1.f - a) + t[i1 * 3 + channel] * a;
}
}

CubeLut parseCubeLut(const QByteArray& text, QString* error)
{
  auto fail = [&](const QString& e) {
    if(error)
      *error = e;
    return CubeLut{};
  };

  CubeLut lut;
  int size1D = 0;
  std::vector<float> data;
  for(QByteArray line : text.split('\n'))
  {
    line = line.trimmed();
    if(line.isEmpty() || line.startsWith('#'))
      continue;
    const auto tok = line.simplified().split(' ');
    const QByteArray& key = tok[0];
    const char c = key[0];
    if((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.')
    {
      float v[3];
      if(!parseFloats(tok, 0, v, 3))
        return fail(QStringLiteral("malformed data line: ") + QString::fromUtf8(line));
      data.insert(data.end(), v, v + 3);
    }
    else if(key == "TITLE")
    {
      lut.title = QString::fromUtf8(line.mid(5).trimmed()).remove(QChar('"'));
    }
    else if(key == "LUT_3D_SIZE" && tok.size() >= 2)
    {
      lut.size = tok[1].toInt();
      if(lut.size < 2 || lut.size > 256)
        return fail(QStringLiteral("LUT_3D_SIZE must be in 2..256"));
    }
    else if(key == "LUT_1D_SIZE" && tok.size() >= 2)
    {
      size1D = tok[1].toInt();
      if(size1D < 2 || size1D > 65536)
        return fail(QStringLiteral("LUT_1D_SIZE must be in 2..65536"));
    }
    else if(key == "DOMAIN_MIN")
    {
      if(!parseFloats(tok, 1, lut.domainMin.data(), 3))
        return fail(QStringLiteral("malformed DOMAIN_MIN"));
    }
    else if(key == "DOMAIN_MAX")
    {
      if(!parseFloats(tok, 1, lut.domainMax.data(), 3))
        return fail(QStringLiteral("malformed DOMAIN_MAX"));
    }
    else if(key == "LUT_1D_INPUT_RANGE" || key == "LUT_3D_INPUT_RANGE")
    {
      float r[2];
      if(!parseFloats(tok, 1, r, 2))
        return fail(QStringLiteral("malformed ") + QString::fromUtf8(key));
      lut.domainMin = {r[0], r[0], r[0]};
      lut.domainMax = {r[1], r[1], r[1]};
    }
  }

  for(int i = 0; i < 3; i++)
    if(!(lut.domainMax[i] > lut.domainMin[i]))
      return fail(QStringLiteral("DOMAIN_MAX must be above DOMAIN_MIN"));

  if(lut.size > 0)
  {
    if(data.size() != std::size_t(lut.size) * lut.size * lut.size * 3)
      return fail(
          QStringLiteral("expected %1 entries, found %2")
              .arg(lut.size * lut.size * lut.size)
              .arg(data.size() / 3));
    lut.rgb = std::move(data);
    return lut;
  }
  if(size1D > 0)
  {
    if(data.size() != std::size_t(size1D) * 3)
      return fail(
          QStringLiteral("expected %1 entries, found %2").arg(size1D).arg(data.size() / 3));
    const int n = std::min(size1D, 65);
    lut.size = n;
    lut.rgb.resize(std::size_t(n) * n * n * 3);
    for(int b = 0; b < n; b++)
      for(int g = 0; g < n; g++)
        for(int r = 0; r < n; r++)
        {
          float* p = lut.rgb.data() + (std::size_t(b) * n * n + g * n + r) * 3;
          p[0] = sample1D(data, size1D, 0, float(r) / float(n - 1));
          p[1] = sample1D(data, size1D, 1, float(g) / float(n - 1));
          p[2] = sample1D(data, size1D, 2, float(b) / float(n - 1));
        }
    return lut;
  }
  return fail(QStringLiteral("no LUT_3D_SIZE or LUT_1D_SIZE"));
}

CubeLut loadCubeLut(const QString& path, QString* error)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
  {
    if(error)
      *error = f.errorString();
    return {};
  }
  return parseCubeLut(f.readAll(), error);
}

namespace
{
std::vector<float> voidAndCluster(int n)
{
  const int N = n * n;
  constexpr double sigma = 1.5;
  std::vector<double> kernel(N);
  for(int y = 0; y < n; y++)
    for(int x = 0; x < n; x++)
    {
      const int dx = std::min(x, n - x), dy = std::min(y, n - y);
      kernel[y * n + x] = std::exp(-(dx * dx + dy * dy) / (2. * sigma * sigma));
    }

  std::vector<char> bits(N, 0);
  std::vector<double> energy(N, 0.);
  auto splat = [&](int idx, double sign) {
    const int px = idx % n, py = idx / n;
    for(int y = 0; y < n; y++)
    {
      const int ky = ((y - py) % n + n) % n;
      for(int x = 0; x < n; x++)
      {
        const int kx = ((x - px) % n + n) % n;
        energy[y * n + x] += sign * kernel[ky * n + kx];
      }
    }
  };
  auto tightestCluster = [&] {
    int best = -1;
    for(int i = 0; i < N; i++)
      if(bits[i] && (best < 0 || energy[i] > energy[best]))
        best = i;
    return best;
  };
  auto largestVoid = [&] {
    int best = -1;
    for(int i = 0; i < N; i++)
      if(!bits[i] && (best < 0 || energy[i] < energy[best]))
        best = i;
    return best;
  };

  std::mt19937 rng{0x5eed};
  const int ones = std::max(1, N / 10);
  for(int placed = 0; placed < ones;)
  {
    const int i = int(rng() % N);
    if(!bits[i])
    {
      bits[i] = 1;
      splat(i, 1.);
      placed++;
    }
  }
  for(int guard = 0; guard < 4 * N; guard++)
  {
    const int c = tightestCluster();
    bits[c] = 0;
    splat(c, -1.);
    const int v = largestVoid();
    bits[v] = 1;
    splat(v, 1.);
    if(v == c)
      break;
  }

  std::vector<int> rank(N, 0);
  const auto prototype = bits;
  const auto prototypeEnergy = energy;
  for(int r = ones - 1; r >= 0; r--)
  {
    const int c = tightestCluster();
    bits[c] = 0;
    splat(c, -1.);
    rank[c] = r;
  }
  bits = prototype;
  energy = prototypeEnergy;
  for(int r = ones; r < N; r++)
  {
    const int v = largestVoid();
    bits[v] = 1;
    splat(v, 1.);
    rank[v] = r;
  }

  std::vector<float> out(N);
  for(int i = 0; i < N; i++)
    out[i] = (float(rank[i]) + 0.5f) / float(N);
  return out;
}
}

const std::vector<float>& blueNoise(int size)
{
  static std::mutex m;
  static std::map<int, std::vector<float>> cache;
  std::lock_guard l{m};
  auto it = cache.find(size);
  if(it == cache.end())
    it = cache.emplace(size, voidAndCluster(size)).first;
  return it->second;
}
}

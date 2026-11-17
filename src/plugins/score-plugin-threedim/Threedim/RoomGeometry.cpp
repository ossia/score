#include "RoomGeometry.hpp"

#include <QFile>
#include <QFileInfo>
#include <QQuaternion>
#include <QTextStream>
#include <QVector3D>

#include <miniply.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Threedim
{
struct RoomGeometry::Node
{
  std::array<float, 3> lo{}, hi{};
  //! Leaf: first triangle in m_order and count; inner: left child, right = left + 1.
  int first{}, count{}, left{-1};
};

RoomGeometry::RoomGeometry() = default;
RoomGeometry::~RoomGeometry() = default;
RoomGeometry::RoomGeometry(RoomGeometry&&) noexcept = default;
RoomGeometry& RoomGeometry::operator=(RoomGeometry&&) noexcept = default;

void RoomGeometry::addTriangle(const std::array<float, 9>& t)
{
  m_triangles.insert(m_triangles.end(), t.begin(), t.end());
}

RoomGeometry RoomGeometry::box(
    float front, float back, float left, float right, float floor, float ceiling)
{
  RoomGeometry g;
  constexpr float far = 1000.f;
  auto ext = [](float d) { return d > 0.f ? d : far; };
  const float x0 = -ext(left), x1 = ext(right), y0 = -ext(floor), y1 = ext(ceiling),
              z0 = -ext(front), z1 = ext(back);
  auto quad = [&](std::array<float, 3> a, std::array<float, 3> b, std::array<float, 3> c,
                  std::array<float, 3> d) {
    g.addTriangle({a[0], a[1], a[2], b[0], b[1], b[2], c[0], c[1], c[2]});
    g.addTriangle({a[0], a[1], a[2], c[0], c[1], c[2], d[0], d[1], d[2]});
  };
  if(front > 0.f)
    quad({x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0});
  if(back > 0.f)
    quad({x0, y0, z1}, {x0, y1, z1}, {x1, y1, z1}, {x1, y0, z1});
  if(left > 0.f)
    quad({x0, y0, z0}, {x0, y1, z0}, {x0, y1, z1}, {x0, y0, z1});
  if(right > 0.f)
    quad({x1, y0, z0}, {x1, y0, z1}, {x1, y1, z1}, {x1, y1, z0});
  if(floor > 0.f)
    quad({x0, y0, z0}, {x0, y0, z1}, {x1, y0, z1}, {x1, y0, z0});
  if(ceiling > 0.f)
    quad({x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1});
  g.build();
  return g;
}

namespace
{
bool loadObj(const QString& path, RoomGeometry& g, QString* error)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    if(error)
      *error = QStringLiteral("cannot open %1").arg(path);
    return false;
  }
  std::vector<std::array<float, 3>> v;
  QTextStream in{&f};
  while(!in.atEnd())
  {
    const QString line = in.readLine().trimmed();
    if(line.startsWith(QLatin1String("v ")))
    {
      const auto p = line.mid(2).split(' ', Qt::SkipEmptyParts);
      if(p.size() >= 3)
        v.push_back({p[0].toFloat(), p[1].toFloat(), p[2].toFloat()});
    }
    else if(line.startsWith(QLatin1String("f ")))
    {
      std::vector<int> idx;
      for(const auto& tok : line.mid(2).split(' ', Qt::SkipEmptyParts))
      {
        int i = tok.section('/', 0, 0).toInt();
        if(i < 0)
          i = int(v.size()) + i + 1;
        idx.push_back(i - 1);
      }
      for(std::size_t k = 2; k < idx.size(); k++)
      {
        const int a = idx[0], b = idx[k - 1], c = idx[k];
        if(a < 0 || b < 0 || c < 0 || a >= (int)v.size() || b >= (int)v.size()
           || c >= (int)v.size())
          continue;
        g.addTriangle(
            {v[a][0], v[a][1], v[a][2], v[b][0], v[b][1], v[b][2], v[c][0], v[c][1],
             v[c][2]});
      }
    }
  }
  return true;
}

bool loadPly(const QString& path, RoomGeometry& g, QString* error)
{
  miniply::PLYReader reader{path.toLocal8Bit().constData()};
  if(!reader.valid())
  {
    if(error)
      *error = QStringLiteral("cannot read %1").arg(path);
    return false;
  }
  std::vector<float> pos;
  std::vector<int> tris;
  uint32_t nv = 0;
  while(reader.has_element())
  {
    if(reader.element_is(miniply::kPLYVertexElement) && reader.load_element())
    {
      uint32_t idx[3];
      if(reader.find_pos(idx))
      {
        nv = reader.num_rows();
        pos.resize(std::size_t(nv) * 3);
        reader.extract_properties(idx, 3, miniply::PLYPropertyType::Float, pos.data());
      }
    }
    else if(reader.element_is(miniply::kPLYFaceElement) && reader.load_element())
    {
      uint32_t idx[1];
      if(reader.find_indices(idx))
      {
        if(reader.requires_triangulation(idx[0]))
        {
          if(pos.empty())
            break;
          tris.resize(std::size_t(reader.num_triangles(idx[0])) * 3);
          reader.extract_triangles(
              idx[0], pos.data(), nv, miniply::PLYPropertyType::Int, tris.data());
        }
        else
        {
          tris.resize(std::size_t(reader.num_rows()) * 3);
          reader.extract_list_property(idx[0], miniply::PLYPropertyType::Int, tris.data());
        }
      }
    }
    reader.next_element();
  }
  for(std::size_t t = 0; t + 2 < tris.size(); t += 3)
  {
    std::array<float, 9> tri;
    bool ok = true;
    for(int k = 0; k < 3; k++)
    {
      const int i = tris[t + k];
      if(i < 0 || std::size_t(i) >= nv)
      {
        ok = false;
        break;
      }
      for(int c = 0; c < 3; c++)
        tri[k * 3 + c] = pos[std::size_t(i) * 3 + c];
    }
    if(ok)
      g.addTriangle(tri);
  }
  return true;
}
}

RoomGeometry RoomGeometry::load(const QString& path, QString* error)
{
  RoomGeometry g;
  const QString ext = QFileInfo{path}.suffix().toLower();
  bool ok = false;
  if(ext == QLatin1String("obj"))
    ok = loadObj(path, g, error);
  else if(ext == QLatin1String("ply"))
    ok = loadPly(path, g, error);
  else if(error)
    *error = QStringLiteral("%1: not an .obj or .ply mesh").arg(path);
  if(!ok)
    return {};
  if(g.empty() && error)
    *error = QStringLiteral("%1 has no faces").arg(path);
  g.build();
  return g;
}

void RoomGeometry::build()
{
  const int n = int(m_triangles.size() / 9);
  m_order.resize(n);
  for(int i = 0; i < n; i++)
    m_order[i] = i;
  m_nodes.clear();
  if(n == 0)
    return;
  std::vector<std::array<float, 3>> centre(n);
  for(int i = 0; i < n; i++)
    for(int c = 0; c < 3; c++)
      centre[i][c] = (m_triangles[i * 9 + c] + m_triangles[i * 9 + 3 + c]
                      + m_triangles[i * 9 + 6 + c])
                     / 3.f;
  m_nodes.reserve(2 * n);
  m_nodes.push_back({});
  struct Job
  {
    int node, first, count;
  };
  std::vector<Job> stack{{0, 0, n}};
  while(!stack.empty())
  {
    const Job j = stack.back();
    stack.pop_back();
    Node nd;
    nd.lo = {1e30f, 1e30f, 1e30f};
    nd.hi = {-1e30f, -1e30f, -1e30f};
    for(int k = j.first; k < j.first + j.count; k++)
      for(int v = 0; v < 3; v++)
        for(int c = 0; c < 3; c++)
        {
          const float x = m_triangles[m_order[k] * 9 + v * 3 + c];
          nd.lo[c] = std::min(nd.lo[c], x);
          nd.hi[c] = std::max(nd.hi[c], x);
        }
    nd.first = j.first;
    nd.count = j.count;
    if(j.count > 4)
    {
      int axis = 0;
      for(int c = 1; c < 3; c++)
        if(nd.hi[c] - nd.lo[c] > nd.hi[axis] - nd.lo[axis])
          axis = c;
      const int mid = j.first + j.count / 2;
      std::nth_element(
          m_order.begin() + j.first, m_order.begin() + mid, m_order.begin() + j.first + j.count,
          [&](int a, int b) { return centre[a][axis] < centre[b][axis]; });
      nd.left = int(m_nodes.size());
      nd.count = 0;
      m_nodes.push_back({});
      m_nodes.push_back({});
      stack.push_back({nd.left, j.first, mid - j.first});
      stack.push_back({nd.left + 1, mid, j.first + j.count - mid});
    }
    m_nodes[j.node] = nd;
  }
}

float RoomGeometry::cast(const std::array<float, 3>& o, const std::array<float, 3>& d) const
{
  if(m_nodes.empty())
    return -1.f;
  std::array<float, 3> inv;
  for(int c = 0; c < 3; c++)
    inv[c] = std::abs(d[c]) > 1e-12f ? 1.f / d[c] : (d[c] >= 0.f ? 1e30f : -1e30f);
  float best = std::numeric_limits<float>::max();
  int stack[64];
  int sp = 0;
  stack[sp++] = 0;
  while(sp > 0)
  {
    const Node& nd = m_nodes[stack[--sp]];
    float t0 = 0.f, t1 = best;
    for(int c = 0; c < 3; c++)
    {
      float a = (nd.lo[c] - o[c]) * inv[c], b = (nd.hi[c] - o[c]) * inv[c];
      if(a > b)
        std::swap(a, b);
      t0 = std::max(t0, a);
      t1 = std::min(t1, b);
    }
    if(t0 > t1 * 1.0000001f + 1e-6f)
      continue;
    if(nd.left >= 0)
    {
      if(sp < 62)
      {
        stack[sp++] = nd.left;
        stack[sp++] = nd.left + 1;
      }
      continue;
    }
    for(int k = nd.first; k < nd.first + nd.count; k++)
    {
      const float* t = &m_triangles[std::size_t(m_order[k]) * 9];
      const float e1[3]{t[3] - t[0], t[4] - t[1], t[5] - t[2]};
      const float e2[3]{t[6] - t[0], t[7] - t[1], t[8] - t[2]};
      const float p[3]{
          d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2],
          d[0] * e2[1] - d[1] * e2[0]};
      const float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
      if(std::abs(det) < 1e-12f)
        continue;
      const float id = 1.f / det;
      const float s[3]{o[0] - t[0], o[1] - t[1], o[2] - t[2]};
      const float u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * id;
      if(u < -1e-6f || u > 1.f + 1e-6f)
        continue;
      const float q[3]{
          s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2],
          s[0] * e1[1] - s[1] * e1[0]};
      const float v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * id;
      if(v < -1e-6f || u + v > 1.f + 1e-6f)
        continue;
      const float tt = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * id;
      if(tt > 1e-6f && tt < best)
        best = tt;
    }
  }
  return best < std::numeric_limits<float>::max() ? best : -1.f;
}

CastResult castDirections(
    const std::vector<float>& dirs, int W, int H, const RoomGeometry& room,
    const std::array<float, 3>& position, const std::array<float, 3>& rotation)
{
  CastResult r;
  const std::size_t n = std::size_t(W) * H;
  r.points.assign(n * 4, 0.f);
  if(dirs.size() < n * 4 || room.empty())
    return r;
  const QQuaternion q = QQuaternion::fromEulerAngles(rotation[0], rotation[1], rotation[2]);
  std::size_t valid = 0, hit = 0;
  for(std::size_t i = 0; i < n; i++)
  {
    const float* d = &dirs[i * 4];
    if(!(d[3] > 0.5f) || !std::isfinite(d[0]) || !std::isfinite(d[1]) || !std::isfinite(d[2]))
      continue;
    valid++;
    QVector3D w = q.rotatedVector(QVector3D(d[0], d[1], d[2]));
    if(w.lengthSquared() < 1e-12f)
      continue;
    w.normalize();
    const std::array<float, 3> dir{w.x(), w.y(), w.z()};
    const float t = room.cast(position, dir);
    if(t <= 0.f)
      continue;
    hit++;
    float* p = &r.points[i * 4];
    for(int c = 0; c < 3; c++)
      p[c] = position[c] + t * dir[c];
    p[3] = 1.f;
  }
  r.hitFraction = valid ? double(hit) / double(valid) : 0.;
  return r;
}
}

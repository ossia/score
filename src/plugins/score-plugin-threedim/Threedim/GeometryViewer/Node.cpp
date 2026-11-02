#include "Node.hpp"

#include <ossia/network/value/value_conversion.hpp>

#include <Threedim/GeometryViewer/Process.hpp>
#include <Threedim/GeometryViewer/SceneDrawer.hpp>

#include <QMatrix4x4>
#include <QVector3D>

#include <cmath>
#include <numbers>

// <windows.h>, which another file of a unity build may include after
// ModelDisplayNode.hpp, defines near and far as nothing
#if defined(near)
#undef near
#undef far
#endif

namespace Gfx::GeometryViewer
{
namespace
{
// Model Display's projections and draw modes
constexpr int projection_uv = 0, projection_barycentric = 5, projection_light = 6,
              projection_color = 7;
constexpr int draw_triangles = 0, draw_points = 1;

//! 1, 2 or 5 times a power of ten, near `v`
float niceStep(float v)
{
  if(!(v > 0.f) || !std::isfinite(v))
    return 1.f;
  const float mag = std::pow(10.f, std::floor(std::log10(v)));
  const float norm = v / mag;
  return mag * (norm < 2.f ? 1.f : norm < 5.f ? 2.f : 5.f);
}
}

void GeometryViewerNode::process(score::gfx::Message&& msg)
{
  // Auto-rotation follows the time of the document: 20 degrees per second
  if(m_spin)
  {
    m_spinAngle = std::fmod(float(msg.token.date.impl / 705600000.) * 20.f, 360.f);
    updateCamera();
  }
  ModelDisplayNode::process(std::move(msg));
}

void GeometryViewerNode::processControl(int32_t port, const ossia::value& v)
{
  switch(port)
  {
    case 2:
      m_mode = ossia::convert<int>(v);
      break;
    case 3:
      m_yaw = ossia::convert<float>(v);
      break;
    case 4:
      m_pitch = ossia::convert<float>(v);
      break;
    case 5:
      m_zoom = std::max(ossia::convert<float>(v), 0.001f);
      break;
    case 6:
      m_center = ossia::convert<ossia::vec3f>(v);
      break;
    case 7:
      m_fov = ossia::convert<float>(v);
      break;
    case 8:
      m_spin = ossia::convert<bool>(v);
      if(!m_spin)
        m_spinAngle = 0.f;
      break;
    case 9:
    {
      const bool autofit = ossia::convert<bool>(v);
      // Turned on: measured again, the geometry may have changed meanwhile
      if(autofit && !m_autofit)
        fit_request++;
      m_autofit = autofit;
      break;
    }
    case 10:
      m_background = ossia::convert<int>(v);
      break;
    case 11:
      m_plane = ossia::convert<bool>(v);
      break;
    case 12:
      m_axes = ossia::convert<bool>(v);
      break;
    case 13:
      m_gizmos = ossia::convert<bool>(v);
      break;
    default:
      return;
  }
  updateCamera();
}

void GeometryViewerNode::process(int32_t port, const ossia::transform3d& v)
{
  ModelDisplayNode::process(port, v);
  if(fit())
    updateCamera();
}

std::unique_ptr<score::gfx::ModelSceneDrawer> GeometryViewerNode::createSceneDrawer() const
{
  return std::make_unique<SceneDrawer>(*this);
}

bool GeometryViewerNode::fitBounds(const ossia::aabb& box)
{
  m_box = box;
  if(!fit())
    return false;
  updateCamera();
  return true;
}

bool GeometryViewerNode::fit()
{
  if(!m_box)
    return false;
  const auto& box = *m_box;
  // The corners where the model matrix puts them
  QMatrix4x4 model{ubo.model};
  model = model.transposed();
  QVector3D lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
  for(int i = 0; i < 8; i++)
  {
    const QVector3D c = model.map(QVector3D{
        (i & 1) ? box.max[0] : box.min[0], (i & 2) ? box.max[1] : box.min[1],
        (i & 4) ? box.max[2] : box.min[2]});
    lo = QVector3D{std::min(lo.x(), c.x()), std::min(lo.y(), c.y()), std::min(lo.z(), c.z())};
    hi = QVector3D{std::max(hi.x(), c.x()), std::max(hi.y(), c.y()), std::max(hi.z(), c.z())};
  }
  const QVector3D mid = (lo + hi) * 0.5f;
  const ossia::vec3f center{mid.x(), mid.y(), mid.z()};
  const float radius = std::max((hi - lo).length() * 0.5f, 1e-4f);
  if(m_fitted && center == m_fitCenter && radius == m_fitRadius && lo.y() == m_fitFloor)
    return false;
  m_fitted = true;
  m_fitCenter = center;
  m_fitRadius = radius;
  m_fitFloor = lo.y();
  return m_autofit;
}

void GeometryViewerNode::updateCamera()
{
  constexpr float deg = std::numbers::pi_v<float> / 180.f;
  const float yaw = (m_yaw + m_spinAngle) * deg, pitch = m_pitch * deg;

  // Fitted: the centre control moves from the middle of the geometry, and a
  // zoom of 1 shows all of it
  const bool fit = m_autofit && m_fitted;
  const float radius = fit ? m_fitRadius : 1.f;
  const ossia::vec3f target{
      m_center[0] + (fit ? m_fitCenter[0] : 0.f), m_center[1] + (fit ? m_fitCenter[1] : 0.f),
      m_center[2] + (fit ? m_fitCenter[2] : 0.f)};
  const float distance
      = radius * 1.1f / std::sin(std::clamp(m_fov, 5.f, 170.f) * 0.5f * deg) / m_zoom;

  // Over the poles the camera keeps going: up is the direction pitch moves
  // the eye in, so it turns over with it
  const float cp = std::cos(pitch), sp = std::sin(pitch);
  center = target;
  position = ossia::vec3f{
      target[0] + distance * cp * std::sin(yaw), target[1] + distance * sp,
      target[2] + distance * cp * std::cos(yaw)};
  up = ossia::vec3f{-sp * std::sin(yaw), cp, -sp * std::cos(yaw)};
  fov = m_fov;
  near = distance * 0.001f;
  far = (distance + radius * 20.f) * 10.f;

  texture_projection = projection_light;
  draw_mode = draw_triangles;
  fallback_pattern = 0;
  prefer_vertex_colors = false;
  switch(Mode(m_mode))
  {
    case Mode::Wireframe:
      texture_projection = projection_barycentric;
      fallback_pattern = 2;
      break;
    case Mode::Points:
      draw_mode = draw_points;
      prefer_vertex_colors = true;
      break;
    case Mode::VertexColors:
      texture_projection = projection_color;
      break;
    case Mode::TextureCoordinates:
      texture_projection = projection_uv;
      fallback_pattern = 1;
      break;
    default:
      break;
  }

  auto& g = guides;
  auto flat = [&](float v) { g.top = g.bottom = {v, v, v, 1.f}; };
  switch(Background(m_background))
  {
    case Background::None:
      g.top = g.bottom = {};
      break;
    case Background::Black:
      flat(0.f);
      break;
    case Background::Dark:
      flat(0.12f);
      break;
    case Background::Grey:
      flat(0.38f);
      break;
    case Background::White:
      flat(0.95f);
      break;
    case Background::Gradient:
    default:
      g.top = {0.21f, 0.22f, 0.25f, 1.f};
      g.bottom = {0.05f, 0.05f, 0.06f, 1.f};
      break;
  }
  g.plane = m_plane;
  g.axes = m_axes;
  g.spacing = niceStep(radius * 0.5f);
  g.origin = {target[0], fit ? m_fitFloor : 0.f, target[2]};

  materialChange();
}
}

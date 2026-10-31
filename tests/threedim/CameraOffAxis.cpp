// Off-axis camera model: lens shift on the FOV form, OpenCV intrinsics, raw
// pose and projection matrices, following
// immersive-analysis/CONVENTIONS.md (pixel centres at integers, v down,
// fy = H / (2 tan(fovY/2)), cx = (W-1)/2 - sx W, cy = (H-1)/2 + sy H).

#include <Gfx/Graph/CameraMath.hpp>
#include <Threedim/Camera.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QMatrix4x4>
#include <QVector4D>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>
#include <string>

using namespace score::gfx;

namespace
{
constexpr float W = 1920.f;
constexpr float H = 1080.f;

QMatrix4x4 unpack(const float src[16])
{
  QMatrix4x4 m;
  std::memcpy(m.data(), src, 16 * sizeof(float));
  return m;
}

bool sameMatrix(const QMatrix4x4& a, const QMatrix4x4& b, float eps = 1e-5f)
{
  for(int i = 0; i < 16; i++)
    if(std::abs(a.constData()[i] - b.constData()[i]) > eps * (1.f + std::abs(b.constData()[i])))
      return false;
  return true;
}

CameraUBOData pack(const ossia::camera_component& cam, const QMatrix4x4& world = {})
{
  CameraUBOData out;
  packCameraUBO(out, cam, world, QSize{int(W), int(H)}, 0.f);
  return out;
}

// OpenCV pixel of a clip-space point, for a W x H image.
QPointF pixelOf(const QVector4D& clip)
{
  const float x = clip.x() / clip.w();
  const float y = clip.y() / clip.w();
  return QPointF((x + 1.f) * 0.5f * W - 0.5f, (1.f - y) * 0.5f * H - 0.5f);
}

// The same point expressed in the OpenCV camera frame and in the GL view frame.
QVector4D glView(QVector3D cv)
{
  return QVector4D(cv.x(), -cv.y(), -cv.z(), 1.f);
}

struct Intrinsics
{
  float fx, fy, cx, cy;
};

Intrinsics fromFov(float fovDeg, float sx, float sy)
{
  const float fy = H / (2.f * std::tan(fovDeg * 0.5f * float(M_PI) / 180.f));
  return {fy, fy, (W - 1.f) / 2.f - sx * W, (H - 1.f) / 2.f + sy * H};
}

ossia::camera_component fovCamera(float fovDeg, float sx, float sy)
{
  ossia::camera_component cam;
  cam.yfov = fovDeg * float(M_PI) / 180.f;
  cam.znear = 0.1f;
  cam.zfar = 100.f;
  cam.shift[0] = sx;
  cam.shift[1] = sy;
  return cam;
}

ossia::camera_component intrinsicsCamera(Intrinsics k)
{
  ossia::camera_component cam;
  cam.znear = 0.1f;
  cam.zfar = 100.f;
  const auto f = frustumFromIntrinsics(k.fx, k.fy, k.cx, k.cy, W, H);
  cam.frustum[0] = f.left;
  cam.frustum[1] = f.right;
  cam.frustum[2] = f.bottom;
  cam.frustum[3] = f.top;
  cam.frustum_set = true;
  return cam;
}

// glFrustum, then the project's reverse-Z depth flip.
QMatrix4x4 analyticFrustum(float l, float r, float b, float t, float n, float f)
{
  QMatrix4x4 m;
  m.frustum(l, r, b, t, n, f);
  flipProjectionDepth(m);
  return m;
}
}

TEST_CASE("intrinsics of a centred FOV camera give the same matrices", "[threedim][camera][offaxis]")
{
  for(float fov : {30.f, 60.f, 90.f})
  {
    const auto a = pack(fovCamera(fov, 0.f, 0.f));
    const auto b = pack(intrinsicsCamera(fromFov(fov, 0.f, 0.f)));
    CHECK(sameMatrix(unpack(a.projection), unpack(b.projection)));
    CHECK(sameMatrix(unpack(a.viewProjection), unpack(b.viewProjection)));
    CHECK(a.params[3] == b.params[3]);
  }
}

TEST_CASE("a shifted FOV camera matches its CONVENTIONS intrinsics", "[threedim][camera][offaxis]")
{
  const float sx = 0.1f, sy = 0.25f;
  const auto a = pack(fovCamera(50.f, sx, sy));
  const auto b = pack(intrinsicsCamera(fromFov(50.f, sx, sy)));
  CHECK(sameMatrix(unpack(a.projection), unpack(b.projection)));

  const float t = std::tan(25.f * float(M_PI) / 180.f);
  const float n = 0.1f;
  const float hw = t * W / H;
  const QMatrix4x4 ref = analyticFrustum(
      n * hw * (-1.f + 2.f * sx), n * hw * (1.f + 2.f * sx), n * t * (-1.f + 2.f * sy),
      n * t * (1.f + 2.f * sy), n, 100.f);
  CHECK(sameMatrix(unpack(a.projection), ref));

  const QPointF centre = pixelOf(unpack(a.projection) * QVector4D(0.f, 0.f, -3.f, 1.f));
  CHECK(std::abs(centre.x() - ((W - 1.f) / 2.f - sx * W)) < 1e-2);
  CHECK(std::abs(centre.y() - ((H - 1.f) / 2.f + sy * H)) < 1e-2);
}

TEST_CASE("an off-axis intrinsics camera projects like OpenCV", "[threedim][camera][offaxis]")
{
  const Intrinsics k{1400.f, 1350.f, 800.f, 700.f};
  const auto ubo = pack(intrinsicsCamera(k));
  const QMatrix4x4 proj = unpack(ubo.projection);

  const float n = 0.1f;
  const QMatrix4x4 ref = analyticFrustum(
      n * (-0.5f - k.cx) / k.fx, n * (W - 0.5f - k.cx) / k.fx,
      n * (k.cy - H + 0.5f) / k.fy, n * (k.cy + 0.5f) / k.fy, n, 100.f);
  CHECK(sameMatrix(proj, ref));

  for(QVector3D p : {QVector3D{0.3f, -0.2f, 2.f}, QVector3D{-1.f, 0.5f, 4.f}, QVector3D{0.f, 0.f, 1.f}})
  {
    const QPointF px = pixelOf(proj * glView(p));
    CHECK(std::abs(px.x() - (k.fx * p.x() / p.z() + k.cx)) < 1e-2);
    CHECK(std::abs(px.y() - (k.fy * p.y() / p.z() + k.cy)) < 1e-2);
  }

  const auto depth = [&](float z) {
    const QVector4D c = proj * QVector4D(0.f, 0.f, z, 1.f);
    return c.z() / c.w();
  };
  CHECK(std::abs(depth(-0.1f) - 1.f) < 1e-4f);
  CHECK(std::abs(depth(-100.f) + 1.f) < 1e-4f);
}

TEST_CASE("a raw OpenGL projection matrix is packed in reverse-Z", "[threedim][camera][offaxis]")
{
  QMatrix4x4 gl;
  gl.frustum(-0.02f, 0.05f, -0.03f, 0.04f, 0.1f, 50.f);
  ossia::camera_component cam;
  std::memcpy(cam.projection_matrix, gl.constData(), sizeof(cam.projection_matrix));
  cam.projection_matrix_set = true;

  const QMatrix4x4 proj = unpack(pack(cam).projection);
  QMatrix4x4 expected = gl;
  flipProjectionDepth(expected);
  CHECK(sameMatrix(proj, expected));
  const QVector4D c = proj * QVector4D(0.f, 0.f, -0.1f, 1.f);
  CHECK(std::abs(c.z() / c.w() - 1.f) < 1e-4f);
}

TEST_CASE("Camera process: intrinsics and an OpenCV pose follow CONVENTIONS", "[threedim][camera][offaxis]")
{
  Threedim::Camera cam;
  const Intrinsics k{1200.f, 1200.f, 900.f, 600.f};
  cam.inputs.projection.value = Threedim::Camera::ins::Intrinsics;
  cam.inputs.intrinsics.value = {k.fx, k.fy, k.cx, k.cy};
  cam.inputs.image_size.value = {W, H};
  cam.inputs.near_plane.value = 0.1f;
  cam.inputs.far_plane.value = 100.f;

  // worldFromProjector: projector at (1, 2, 3), turned 90 degrees about world +Y,
  // OpenCV local axes.
  QMatrix4x4 worldFromCv;
  worldFromCv.translate(1.f, 2.f, 3.f);
  worldFromCv.rotate(90.f, 0.f, 1.f, 0.f);
  std::array<float, 16> m;
  std::memcpy(m.data(), worldFromCv.constData(), sizeof(m));
  cam.inputs.pose.value = Threedim::Camera::ins::WorldFromCameraCV;
  std::string text;
  for(float f : m)
    text += std::to_string(f) + " ";
  cam.inputs.pose_matrix.value = text;
  cam.rebuild();

  const auto& node = (*cam.m_state->roots)[0];
  const ossia::scene_transform* xf{};
  const ossia::camera_component* cc{};
  for(const auto& c : *node->children)
  {
    if(auto* x = ossia::get_if<ossia::scene_transform>(&c))
      xf = x;
    else if(auto* p = ossia::get_if<ossia::camera_component_ptr>(&c))
      cc = p->get();
  }
  REQUIRE(xf);
  REQUIRE(cc);

  QMatrix4x4 world;
  world.translate(xf->translation[0], xf->translation[1], xf->translation[2]);
  world.rotate(QQuaternion(xf->rotation[3], xf->rotation[0], xf->rotation[1], xf->rotation[2]));

  CameraUBOData ubo;
  packCameraUBO(ubo, *cc, world, QSize{640, 480}, 0.f);
  const QMatrix4x4 vp = unpack(ubo.viewProjection);

  for(QVector3D local : {QVector3D{0.f, 0.f, 2.f}, QVector3D{0.4f, -0.3f, 3.f}, QVector3D{-0.5f, 0.6f, 1.5f}})
  {
    const QVector3D worldPt = worldFromCv.map(local);
    const QPointF px = pixelOf(vp * QVector4D(worldPt, 1.f));
    CHECK(std::abs(px.x() - (k.fx * local.x() / local.z() + k.cx)) < 2e-2);
    CHECK(std::abs(px.y() - (k.fy * local.y() / local.z() + k.cy)) < 2e-2);
  }
}

TEST_CASE("Camera process: lens shift reaches the component", "[threedim][camera][offaxis]")
{
  Threedim::Camera cam;
  cam.inputs.shift.value = {0.f, 0.5f};
  cam.rebuild();
  const auto& node = (*cam.m_state->roots)[0];
  for(const auto& c : *node->children)
    if(auto* p = ossia::get_if<ossia::camera_component_ptr>(&c))
    {
      CHECK((*p)->shift[1] == 0.5f);
      CHECK(!(*p)->frustum_set);
      CHECK(!(*p)->projection_matrix_set);
    }
}

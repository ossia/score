// Off-axis camera model: lens shift on the FOV form, OpenCV intrinsics (pixel
// centres at integers, v down, fy = H / (2 tan(fovY/2)), cx = (W-1)/2 - sx W,
// cy = (H-1)/2 + sy H), raw pose and projection matrices, the Screen mode and
// the camera switch's blending of projections.

#include <Gfx/Graph/CameraMath.hpp>
#include <Threedim/Camera.hpp>
#include <Threedim/CameraSwitch.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QMatrix4x4>
#include <QVector2D>
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

std::string rows(const QMatrix4x4& m)
{
  std::string t;
  for(int r = 0; r < 4; r++)
    for(int c = 0; c < 4; c++)
      t += std::to_string(m(r, c)) + " ";
  return t;
}

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

TEST_CASE("a shifted FOV camera matches its intrinsics", "[threedim][camera][offaxis]")
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
  auto ndc = [&](QVector3D p) {
    const QVector4D c = proj * QVector4D(p, 1.f);
    return QVector3D(c.x() / c.w(), c.y() / c.w(), c.z() / c.w());
  };
  CHECK(std::abs(ndc({0.f, 0.f, -0.1f}).z() - 1.f) < 1e-4f);
  CHECK(std::abs(ndc({0.f, 0.f, -50.f}).z() + 1.f) < 1e-3f);
  const QVector3D lb = ndc({-0.02f, -0.03f, -0.1f});
  CHECK(std::abs(lb.x() + 1.f) < 1e-5f);
  CHECK(std::abs(lb.y() + 1.f) < 1e-5f);
  const QVector3D rt = ndc({0.05f * 20.f, 0.04f * 20.f, -2.f});
  CHECK(std::abs(rt.x() - 1.f) < 1e-5f);
  CHECK(std::abs(rt.y() - 1.f) < 1e-5f);
}

TEST_CASE("Camera process: intrinsics and an OpenCV pose project like OpenCV", "[threedim][camera][offaxis]")
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
  cam.inputs.pose.value = Threedim::Camera::ins::WorldFromCameraCV;
  cam.inputs.pose_matrix.value = rows(worldFromCv);
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
  int found = 0;
  for(const auto& c : *node->children)
    if(auto* p = ossia::get_if<ossia::camera_component_ptr>(&c))
    {
      found++;
      CHECK((*p)->shift[1] == 0.5f);
      CHECK(!(*p)->frustum_set);
      CHECK(!(*p)->projection_matrix_set);
    }
  CHECK(found == 1);
}

TEST_CASE("Camera process: matrix text is read row by row", "[threedim][camera]")
{
  const auto m = Threedim::parseMatrix("[1, 2e-1, -3.5, +4; 5 6 7 8 9 10 11 12 13 14 15 1.5E2]");
  REQUIRE(m);
  CHECK((*m)[0] == 1.f);
  CHECK(std::abs((*m)[4] - 0.2f) < 1e-7f);
  CHECK((*m)[8] == -3.5f);
  CHECK((*m)[12] == 4.f);
  CHECK((*m)[1] == 5.f);
  CHECK((*m)[3] == 13.f);
  CHECK((*m)[15] == 150.f);

  QMatrix4x4 q;
  q.translate(1.f, 2.f, 3.f);
  q.rotate(40.f, 0.f, 0.f, 1.f);
  const auto back = Threedim::parseMatrix(rows(q));
  REQUIRE(back);
  for(int i = 0; i < 16; i++)
    CHECK(std::abs((*back)[i] - q.constData()[i]) < 1e-5f);

  CHECK_FALSE(Threedim::parseMatrix("1 2 3"));
  CHECK_FALSE(Threedim::parseMatrix("1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1 7"));
  CHECK_FALSE(Threedim::parseMatrix("1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1-2"));

  Threedim::ParsedMatrix cache;
  CHECK((*cache("2 0 0 0 0 2 0 0 0 0 2 0 0 0 0 1"))[0] == 2.f);
  CHECK((*cache("3 0 0 0 0 3 0 0 0 0 3 0 0 0 0 1"))[0] == 3.f);
  CHECK_FALSE(cache("nope"));
}

TEST_CASE("Camera process: bad matrices and focal lengths fall back", "[threedim][camera]")
{
  Threedim::Camera lookAt;
  lookAt.inputs.eye.value = {1.f, 2.f, 3.f};
  lookAt.inputs.target.value = {0.f, 0.f, 0.f};

  for(const char* text : {"1 2 3", "1 0 0 0 0 1 0 0 0 0 1 0 0.5 0 0 1"})
  {
    CAPTURE(text);
    Threedim::Camera cam;
    cam.inputs.eye.value = lookAt.inputs.eye.value;
    cam.inputs.target.value = lookAt.inputs.target.value;
    cam.inputs.pose.value = Threedim::Camera::ins::WorldFromCameraGL;
    cam.inputs.pose_matrix.value = text;
    CHECK(sameMatrix(cam.worldTransform(), lookAt.worldTransform()));
    CHECK((cam.position() - QVector3D(1.f, 2.f, 3.f)).length() < 1e-6f);
  }

  Threedim::Camera proj;
  proj.inputs.projection.value = Threedim::Camera::ins::ProjectionMatrix;
  proj.inputs.projection_matrix.value = "not a matrix";
  proj.inputs.shift.value = {0.1f, 0.f};
  ossia::camera_component c;
  proj.fillProjection(c);
  CHECK_FALSE(c.projection_matrix_set);
  CHECK(c.shift[0] == 0.1f);

  for(float fx : {0.f, -800.f})
  {
    Threedim::Camera intr;
    intr.inputs.projection.value = Threedim::Camera::ins::Intrinsics;
    intr.inputs.intrinsics.value = {fx, 1000.f, 959.5f, 539.5f};
    ossia::camera_component k;
    intr.fillProjection(k);
    CHECK_FALSE(k.frustum_set);
  }
}

TEST_CASE("Camera process: a raw matrix reports its own planes and kind", "[threedim][camera]")
{
  QMatrix4x4 persp;
  persp.perspective(50.f, 1.5f, 0.25f, 40.f);
  Threedim::Camera cam;
  cam.inputs.projection.value = Threedim::Camera::ins::ProjectionMatrix;
  cam.inputs.projection_matrix.value = rows(persp);
  ossia::camera_component c;
  cam.fillProjection(c);
  REQUIRE(c.projection_matrix_set);
  CHECK(c.projection == ossia::camera_projection::perspective);
  CHECK(std::abs(c.znear - 0.25f) < 1e-4f);
  CHECK(std::abs(c.zfar - 40.f) < 1e-2f);
  CHECK(std::abs(c.yfov - 50.f * float(M_PI) / 180.f) < 1e-4f);

  QMatrix4x4 ortho;
  ortho.ortho(-3.f, 3.f, -2.f, 2.f, 0.5f, 20.f);
  cam.inputs.projection_matrix.value = rows(ortho);
  ossia::camera_component o;
  cam.fillProjection(o);
  REQUIRE(o.projection_matrix_set);
  CHECK(o.projection == ossia::camera_projection::orthographic);
  CHECK(std::abs(o.znear - 0.5f) < 1e-4f);
  CHECK(std::abs(o.zfar - 20.f) < 1e-3f);
  CHECK(std::abs(o.xmag - 3.f) < 1e-5f);
  CHECK(std::abs(o.ymag - 2.f) < 1e-5f);
  QMatrix4x4 packed;
  CHECK(cameraProjection(packed, o, 1.f) == CameraProjectionMode::Orthographic);
}

TEST_CASE("Camera process: a Screen camera puts the screen corners on the image corners", "[threedim][camera][offaxis]")
{
  Threedim::Camera cam;
  cam.inputs.projection.value = Threedim::Camera::ins::Screen;
  cam.inputs.eye.value = {0.3f, 1.6f, 0.2f};
  cam.inputs.screen_pa.value = {-1.5f, 0.f, 1.5f};
  cam.inputs.screen_pb.value = {-1.5f, 0.f, -1.5f};
  cam.inputs.screen_pc.value = {-1.5f, 3.f, 1.5f};
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
  REQUIRE(cc->frustum_set);
  CHECK(std::abs(cc->frustum[0] + 1.3f / 1.8f) < 1e-6f);
  CHECK(std::abs(cc->frustum[1] - 1.7f / 1.8f) < 1e-6f);
  CHECK(std::abs(cc->frustum[2] + 1.6f / 1.8f) < 1e-6f);
  CHECK(std::abs(cc->frustum[3] - 1.4f / 1.8f) < 1e-6f);
  QMatrix4x4 world;
  world.translate(xf->translation[0], xf->translation[1], xf->translation[2]);
  world.rotate(QQuaternion(xf->rotation[3], xf->rotation[0], xf->rotation[1], xf->rotation[2]));
  CameraUBOData ubo;
  packCameraUBO(ubo, *cc, world, QSize{640, 480}, 0.f);
  const QMatrix4x4 vp = unpack(ubo.viewProjection);
  auto ndc = [&](QVector3D p) {
    const QVector4D c = vp * QVector4D(p, 1.f);
    return QVector2D(c.x() / c.w(), c.y() / c.w());
  };
  CHECK((ndc({-1.5f, 0.f, 1.5f}) - QVector2D(-1.f, -1.f)).length() < 1e-4f);
  CHECK((ndc({-1.5f, 0.f, -1.5f}) - QVector2D(1.f, -1.f)).length() < 1e-4f);
  CHECK((ndc({-1.5f, 3.f, 1.5f}) - QVector2D(-1.f, 1.f)).length() < 1e-4f);
  CHECK((ndc({-1.5f, 3.f, -1.5f}) - QVector2D(1.f, 1.f)).length() < 1e-4f);
  CHECK((ndc({-1.5f, 1.2f, 0.4f}) - QVector2D(2.f * (1.5f - 0.4f) / 3.f - 1.f, 2.f * 1.2f / 3.f - 1.f)).length() < 1e-4f);

  cam.inputs.eye.value = {-1.5f, 1.f, 0.f};
  CHECK_FALSE(cam.screenValid());
}

TEST_CASE("frustum tangents of a FOV camera with lens shift", "[threedim][camera]")
{
  ossia::camera_component c;
  c.yfov = 2.f * std::atan(0.5f);
  c.shift[0] = 0.25f;
  c.shift[1] = -0.5f;
  float t[4];
  REQUIRE(cameraFrustumTangents(c, 2.f, t));
  CHECK(std::abs(t[0] - (0.5f - 1.f)) < 1e-6f);
  CHECK(std::abs(t[1] - (0.5f + 1.f)) < 1e-6f);
  CHECK(std::abs(t[2] - (-0.5f - 0.5f)) < 1e-6f);
  CHECK(std::abs(t[3] - (-0.5f + 0.5f)) < 1e-6f);
  CHECK_FALSE(cameraFrustumTangents(c, 0.f, t));
  c.projection = ossia::camera_projection::orthographic;
  CHECK_FALSE(cameraFrustumTangents(c, 2.f, t));

  QMatrix4x4 m;
  setReverseZFrustum(m, 1.f, -1.f, -1.f, 1.f, 0.1f, 10.f);
  CHECK(m.isIdentity());
}

TEST_CASE("camera switch: projections blend without a jump", "[threedim][camera][switch]")
{
  using S = Threedim::CameraSwitch;
  ossia::camera_component cams[4];
  cams[0].frustum_set = true;
  cams[0].frustum[0] = -1.f;
  cams[0].frustum[1] = 1.f;
  cams[0].frustum[2] = -0.25f;
  cams[0].frustum[3] = 0.75f;
  cams[1].frustum_set = true;
  cams[1].frustum[0] = -0.5f;
  cams[1].frustum[1] = 1.5f;
  cams[1].frustum[2] = -0.5f;
  cams[1].frustum[3] = 0.5f;
  {
    ossia::camera_component out;
    S::blendProjection(cams, {0.25f, 0.75f, 0.f, 0.f}, out);
    REQUIRE(out.frustum_set);
    CHECK(std::abs(out.frustum[0] - (0.25f * -1.f + 0.75f * -0.5f)) < 1e-6f);
    CHECK(std::abs(out.frustum[1] - (0.25f * 1.f + 0.75f * 1.5f)) < 1e-6f);
    CHECK(std::abs(out.frustum[2] - (0.25f * -0.25f + 0.75f * -0.5f)) < 1e-6f);
    CHECK(std::abs(out.frustum[3] - (0.25f * 0.75f + 0.75f * 0.5f)) < 1e-6f);
    CHECK_FALSE(out.projection_matrix_set);
  }

  ossia::camera_component fov;
  fov.yfov = 2.f * std::atan(0.5f);
  fov.shift[1] = 0.25f;
  ossia::camera_component mix[4]{cams[0], fov};
  float alone[4];
  REQUIRE(cameraFrustumTangents(fov, 2.f, alone));
  {
    ossia::camera_component out;
    S::blendProjection(mix, {1e-4f, 1.f - 1e-4f, 0.f, 0.f}, out);
    REQUIRE(out.frustum_set);
    for(int k = 0; k < 4; k++)
      CHECK(std::abs(out.frustum[k] - alone[k]) < 1e-3f);
  }
  {
    ossia::camera_component out;
    S::blendProjection(mix, {0.5f, 0.5f, 0.f, 0.f}, out);
    REQUIRE(out.frustum_set);
    for(int k = 0; k < 4; k++)
      CHECK(std::abs(out.frustum[k] - 0.5f * (cams[0].frustum[k] + alone[k])) < 1e-6f);
  }

  ossia::camera_component mats[4];
  for(int i = 0; i < 2; i++)
  {
    mats[i].projection_matrix_set = true;
    for(int k = 0; k < 16; k++)
      mats[i].projection_matrix[k] = float(i * 16 + k);
  }
  {
    ossia::camera_component out;
    S::blendProjection(mats, {0.5f, 0.5f, 0.f, 0.f}, out);
    REQUIRE(out.projection_matrix_set);
    CHECK(out.projection_matrix[3] == 0.5f * (3.f + 19.f));
  }
  mats[1].projection_matrix_set = false;
  {
    ossia::camera_component out;
    S::blendProjection(mats, {0.5f, 0.5f, 0.f, 0.f}, out);
    CHECK_FALSE(out.projection_matrix_set);
  }
}

TEST_CASE("Camera process: OpenGL world-from-camera and view poses", "[threedim][camera]")
{
  QMatrix4x4 worldFromGl;
  worldFromGl.translate(1.f, 2.f, 3.f);
  worldFromGl.rotate(30.f, 0.f, 1.f, 0.f);
  worldFromGl.rotate(-20.f, 1.f, 0.f, 0.f);

  Threedim::Camera gl;
  gl.inputs.pose.value = Threedim::Camera::ins::WorldFromCameraGL;
  gl.inputs.pose_matrix.value = rows(worldFromGl);
  CHECK(sameMatrix(gl.worldTransform(), worldFromGl, 1e-4f));
  CHECK((gl.position() - QVector3D(1.f, 2.f, 3.f)).length() < 1e-4f);

  Threedim::Camera view;
  view.inputs.pose.value = Threedim::Camera::ins::ViewGL;
  view.inputs.pose_matrix.value = rows(worldFromGl.inverted());
  CHECK(sameMatrix(view.worldTransform(), worldFromGl, 1e-4f));

  const QVector3D fwd = view.orientation().rotatedVector(QVector3D(0.f, 0.f, -1.f));
  const QVector3D want = worldFromGl.mapVector(QVector3D(0.f, 0.f, -1.f));
  CHECK((fwd - want).length() < 1e-4f);
}

TEST_CASE("Camera process: a Screen behind the eye falls back to Look at", "[threedim][camera][offaxis]")
{
  Threedim::Camera cam;
  cam.inputs.projection.value = Threedim::Camera::ins::Screen;
  cam.inputs.eye.value = {0.f, 1.f, -5.f};
  cam.inputs.target.value = {0.f, 1.f, -10.f};
  cam.inputs.screen_pa.value = {-1.f, 0.f, -1.f};
  cam.inputs.screen_pb.value = {1.f, 0.f, -1.f};
  cam.inputs.screen_pc.value = {-1.f, 2.f, -1.f};
  CHECK(cam.screenMode());
  CHECK_FALSE(cam.screenValid());

  ossia::camera_component c;
  cam.fillProjection(c);
  CHECK_FALSE(c.frustum_set);

  Threedim::Camera lookAt;
  lookAt.inputs.eye.value = cam.inputs.eye.value;
  lookAt.inputs.target.value = cam.inputs.target.value;
  CHECK(sameMatrix(cam.worldTransform(), lookAt.worldTransform()));

  cam.inputs.eye.value = {0.f, 1.f, 2.f};
  CHECK(cam.screenValid());
}

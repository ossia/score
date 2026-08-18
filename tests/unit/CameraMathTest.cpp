// UNIT — Gfx/Graph/CameraMath.{hpp,cpp}: the reverse-Z projection and the
// std140 camera UBO every scene shader binds.
//
// A sign or row/column slip here inverts depth across the whole renderer:
// geometry still draws, it just occludes backwards, which is precisely what a
// "did anything render" sweep cannot see. The field offsets are pinned because
// out-of-tree tester shaders have shipped a 208-byte camera struct against this
// 240-byte one.

#include <Gfx/Graph/CameraMath.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QMatrix4x4>
#include <QVector4D>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>

using namespace score::gfx;
using Catch::Approx;

namespace
{
float ndcZ(const QMatrix4x4& proj, float viewZ)
{
  const QVector4D clip = proj * QVector4D{0.f, 0.f, viewZ, 1.f};
  return clip.z() / clip.w();
}
}

TEST_CASE("setReverseZPerspective: near maps to +1 and far to -1", "[camera]")
{
  QMatrix4x4 m;
  setReverseZPerspective(m, 60.f, 16.f / 9.f, 0.1f, 100.f);

  const float atNear = ndcZ(m, -0.1f);
  const float atFar = ndcZ(m, -100.f);

  CHECK(atNear == Approx(1.f).margin(1e-4));
  CHECK(atFar == Approx(-1.f).margin(1e-4));
  CHECK(atNear > atFar);
}

TEST_CASE("setReverseZPerspective: ndc depth decreases monotonically with distance", "[camera]")
{
  QMatrix4x4 m;
  setReverseZPerspective(m, 60.f, 1.f, 0.1f, 100.f);

  float prev = ndcZ(m, -0.1f);
  for(int i = 1; i <= 200; i++)
  {
    const float z = -0.1f - (99.9f * float(i) / 200.f);
    const float cur = ndcZ(m, z);
    CHECK(cur < prev);
    prev = cur;
  }
}

TEST_CASE("setReverseZPerspective: aspect only scales the x row", "[camera]")
{
  QMatrix4x4 a, b;
  setReverseZPerspective(a, 60.f, 1.f, 0.1f, 100.f);
  setReverseZPerspective(b, 60.f, 2.f, 0.1f, 100.f);

  CHECK(b(0, 0) == Approx(a(0, 0) / 2.f));
  CHECK(b(1, 1) == Approx(a(1, 1)));
  CHECK(b(2, 2) == Approx(a(2, 2)));
  CHECK(b(2, 3) == Approx(a(2, 3)));
  CHECK(b(3, 2) == Approx(-1.f));
  CHECK(b(3, 3) == Approx(0.f));
}

TEST_CASE("setReverseZPerspective: degenerate parameters leave the identity", "[camera]")
{
  QMatrix4x4 identity;

  const auto build = [](float fov, float aspect, float n, float f) {
    QMatrix4x4 m;
    m.translate(9.f, 9.f, 9.f);
    setReverseZPerspective(m, fov, aspect, n, f);
    return m;
  };

  CHECK(build(60.f, 1.f, 1.f, 1.f) == identity);
  CHECK(build(60.f, 0.f, 0.1f, 100.f) == identity);
  CHECK(build(0.f, 1.f, 0.1f, 100.f) == identity);
  CHECK_FALSE(build(60.f, 1.f, 0.1f, 100.f) == identity);
}

TEST_CASE("writeMat4 emits column-major floats", "[camera]")
{
  QMatrix4x4 m;
  m.setToIdentity();
  m.translate(1.f, 2.f, 3.f);

  float dst[16]{};
  writeMat4(dst, m);

  for(int i = 0; i < 16; i++)
    CHECK(dst[i] == Approx(m.constData()[i]));

  // Column-major: the translation lands in the last column, i.e. [12..14].
  CHECK(dst[12] == Approx(1.f));
  CHECK(dst[13] == Approx(2.f));
  CHECK(dst[14] == Approx(3.f));
  CHECK(dst[15] == Approx(1.f));
  CHECK(dst[3] == Approx(0.f));
}

TEST_CASE("CameraUBOData: the std140 field offsets shaders depend on", "[camera][abi]")
{
  CHECK(sizeof(CameraUBOData) == 240);
  CHECK(offsetof(CameraUBOData, view) == 0);
  CHECK(offsetof(CameraUBOData, projection) == 64);
  CHECK(offsetof(CameraUBOData, viewProjection) == 128);
  CHECK(offsetof(CameraUBOData, cameraPosition) == 192);
  CHECK(offsetof(CameraUBOData, renderSize) == 208);
  CHECK(offsetof(CameraUBOData, params) == 224);
}

TEST_CASE("packCameraUBO: eye, view and viewProjection", "[camera]")
{
  ossia::camera_component cam;
  cam.yfov = 0.7853981f;
  cam.znear = 0.25f;
  cam.zfar = 750.f;

  QMatrix4x4 world;
  world.translate(5.f, 0.f, 0.f);

  CameraUBOData out{};
  packCameraUBO(out, cam, world, QSize{800, 400}, 1.5f);

  CHECK(out.cameraPosition[0] == Approx(5.f));
  CHECK(out.cameraPosition[1] == Approx(0.f));
  CHECK(out.cameraPosition[2] == Approx(0.f));
  CHECK(out.cameraPosition[3] == Approx(0.f));

  const QMatrix4x4 view(out.view, 4, 4);
  const QVector3D seen = view.map(QVector3D{0.f, 0.f, 0.f});
  CHECK(seen.x() == Approx(-5.f));

  QMatrix4x4 expectedProj;
  setReverseZPerspective(
      expectedProj, cam.yfov * (180.f / float(M_PI)), 2.f, cam.znear, cam.zfar);
  for(int i = 0; i < 16; i++)
    CHECK(out.projection[i] == Approx(expectedProj.constData()[i]));

  const QMatrix4x4 vp(out.viewProjection, 4, 4);
  const QMatrix4x4 expectedVp = expectedProj * view;
  for(int i = 0; i < 16; i++)
    CHECK(vp.constData()[i] == Approx(expectedVp.constData()[i]));

  CHECK(out.renderSize[0] == Approx(800.f));
  CHECK(out.renderSize[1] == Approx(400.f));
  CHECK(out.params[0] == Approx(1.5f));
  CHECK(out.params[1] == Approx(0.25f));
  CHECK(out.params[2] == Approx(750.f));
}

TEST_CASE("packCameraUBO: aspect comes from renderSize unless overridden", "[camera]")
{
  ossia::camera_component cam;
  cam.aspect_ratio = 3.f;

  QMatrix4x4 world;

  CameraUBOData fromSize{};
  packCameraUBO(fromSize, cam, world, QSize{1000, 250}, 0.f);
  QMatrix4x4 expect4;
  setReverseZPerspective(
      expect4, cam.yfov * (180.f / float(M_PI)), 4.f, cam.znear, cam.zfar);
  CHECK(fromSize.projection[0] == Approx(expect4.constData()[0]));

  CameraUBOData overridden{};
  packCameraUBO(overridden, cam, world, QSize{1000, 250}, 0.f, 8.f);
  QMatrix4x4 expect8;
  setReverseZPerspective(
      expect8, cam.yfov * (180.f / float(M_PI)), 8.f, cam.znear, cam.zfar);
  CHECK(overridden.projection[0] == Approx(expect8.constData()[0]));

  // A zero-height target falls back to the component's own aspect_ratio.
  CameraUBOData degenerate{};
  packCameraUBO(degenerate, cam, world, QSize{1000, 0}, 0.f);
  QMatrix4x4 expect3;
  setReverseZPerspective(
      expect3, cam.yfov * (180.f / float(M_PI)), 3.f, cam.znear, cam.zfar);
  CHECK(degenerate.projection[0] == Approx(expect3.constData()[0]));
}

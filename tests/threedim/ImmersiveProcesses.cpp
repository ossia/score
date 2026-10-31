// Projector to Viewer Frustum reads its calibration file once per change, and
// Camera Switch crossfades off-axis cameras without a jump.

#include <Threedim/CalibrationFiles.hpp>
#include <Threedim/CameraSwitch.hpp>
#include <Threedim/ViewerFrustum.hpp>

#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using Catch::Approx;

namespace
{
Threedim::CalibrationFile wall(const std::string& base, int w, int h)
{
  Threedim::CalibrationFile c;
  c.base = base;
  c.width = w;
  c.height = h;
  c.xyz.resize(std::size_t(w) * h * 4);
  for(int v = 0; v < h; v++)
    for(int u = 0; u < w; u++)
    {
      float* p = c.xyz.data() + (std::size_t(v) * w + u) * 4;
      p[0] = -1.f + 2.f * (u + 0.5f) / w;
      p[1] = 2.f - 1.5f * (v + 0.5f) / h;
      p[2] = -3.f;
      p[3] = 1.f;
    }
  return c;
}
}

TEST_CASE("Projector to Viewer Frustum caches its calibration file", "[offaxis][viewer]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const std::string base = dir.filePath("wall").toStdString();
  REQUIRE(Threedim::saveCalibration(wall(base, 64, 48)));

  Threedim::ProjectorViewerFrustum f;
  f.inputs.file.value = base + ".json";
  f.inputs.continuous.value = true;
  f.inputs.viewer.value = {0.f, 1.f, 0.f};
  f();
  INFO(f.outputs.status.value);
  REQUIRE(f.outputs.status.value.rfind("Done", 0) == 0);
  CHECK(f.outputs.planar.value);
  const float* first = f.calibration(f.inputs.file.value).xyz.data();
  REQUIRE(first);

  f.inputs.viewer.value = {0.2f, 1.f, 0.f};
  const float left = f.outputs.frustum.value[0];
  f();
  CHECK(f.calibration(f.inputs.file.value).xyz.data() == first);
  CHECK(f.outputs.frustum.value[0] != left);

  REQUIRE(Threedim::saveCalibration(wall(base, 32, 24)));
  f();
  CHECK(f.calibration(f.inputs.file.value).width == 32);
}

TEST_CASE("Camera Switch blends off-axis frusta", "[camera][offaxis]")
{
  ossia::camera_component a, b;
  a.frustum[0] = -1.f;
  a.frustum[1] = 0.5f;
  a.frustum[2] = -0.5f;
  a.frustum[3] = 0.5f;
  a.frustum_set = true;
  b.frustum[0] = -0.5f;
  b.frustum[1] = 1.f;
  b.frustum[2] = -0.25f;
  b.frustum[3] = 0.75f;
  b.frustum_set = true;
  const ossia::camera_component cams[4]{a, b, {}, {}};

  auto blended = [&](float wb) {
    const float w[4]{1.f - wb, wb, 0.f, 0.f};
    ossia::camera_component out;
    out.projection = ossia::camera_projection::perspective;
    Threedim::CameraSwitch::blendProjection(cams, w, out);
    return out;
  };
  const auto below = blended(0.49f), above = blended(0.51f);
  REQUIRE(below.frustum_set);
  for(int k = 0; k < 4; k++)
    CHECK(std::abs(below.frustum[k] - above.frustum[k]) < 0.02f);
  CHECK(blended(0.5f).frustum[0] == Approx(-0.75f));
  CHECK(blended(0.f).frustum[3] == Approx(0.5f));

  ossia::camera_component fov;
  fov.yfov = 2.f * std::atan(0.5f);
  fov.aspect_ratio = 2.f;
  fov.aspect_ratio_set = true;
  fov.shift[1] = 0.25f;
  float t[4];
  REQUIRE(Threedim::CameraSwitch::frustumTangents(fov, t));
  CHECK(t[0] == Approx(-1.f));
  CHECK(t[1] == Approx(1.f));
  CHECK(t[2] == Approx(-0.25f));
  CHECK(t[3] == Approx(0.75f));

  fov.aspect_ratio_set = false;
  CHECK_FALSE(Threedim::CameraSwitch::frustumTangents(fov, t));
  const ossia::camera_component mixed[4]{a, fov, {}, {}};
  const float w[4]{0.5f, 0.5f, 0.f, 0.f};
  ossia::camera_component out;
  Threedim::CameraSwitch::blendProjection(mixed, w, out);
  CHECK_FALSE(out.frustum_set);
}

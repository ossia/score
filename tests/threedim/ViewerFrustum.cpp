// Projector to Viewer Frustum: its calibration read once per change on the
// worker, its matrices as a Camera in Matrix mode takes them; Viewer From
// Pose's filter on every tick.

#include <Gfx/Graph/CameraMath.hpp>
#include <Threedim/CalibrationFiles.hpp>
#include <Threedim/Camera.hpp>
#include <Threedim/ViewerFromPose.hpp>
#include <Threedim/ViewerFrustum.hpp>

#include <QFile>
#include <QTemporaryDir>
#include <QVector4D>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>

using Catch::Approx;
using Threedim::ProjectorViewerFrustum;

namespace
{
//! A wall x in [-1, 1], y in [0.5, 2], z = -3.
Threedim::CalibrationFile wall(const QString& base, int w, int h)
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

void inPlace(ProjectorViewerFrustum& f)
{
  f.worker.request = [](auto state, uint64_t id, auto job) {
    ProjectorViewerFrustum::worker::work(std::move(state), id, std::move(job));
  };
}

void run(ProjectorViewerFrustum& f)
{
  f();
  f();
}
}

TEST_CASE("Projector to Viewer Frustum reads its calibration once per change", "[offaxis][viewer]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString base = dir.filePath("wall");
  REQUIRE(Threedim::saveCalibration(wall(base, 64, 48)));

  ProjectorViewerFrustum f{};
  inPlace(f);
  f.inputs.file.value = QString(base + ".json").toStdString();
  f.inputs.continuous.value = true;
  f.inputs.viewer.value = {0.f, 1.f, 0.f};
  run(f);
  INFO(f.outputs.status.value);
  REQUIRE(f.outputs.status.value.starts_with("Done"));
  CHECK(f.outputs.planar.value);
  CHECK(f.cache().loads == 1);

  const float left = f.outputs.frustum.value[0];
  f.inputs.viewer.value = {0.2f, 1.f, 0.f};
  run(f);
  CHECK(f.outputs.frustum.value[0] != left);
  CHECK(f.cache().loads == 1);

  for(int i = 0; i < ProjectorViewerFrustum::recheckFrames; i++)
    f();
  CHECK(f.cache().loads == 1);

  REQUIRE(QFile::remove(base + ".json"));
  REQUIRE(Threedim::saveCalibration(wall(base, 32, 24)));
  for(int i = 0; i < ProjectorViewerFrustum::recheckFrames + 1; i++)
    f();
  CHECK(f.cache().loads == 2);
  CHECK(f.cache().calibration->width == 32);
}

TEST_CASE(
    "Projector to Viewer Frustum gives OpenGL matrices a Camera reads",
    "[offaxis][viewer][camera]")
{
  QTemporaryDir dir;
  const QString base = dir.filePath("wall");
  REQUIRE(Threedim::saveCalibration(wall(base, 64, 48)));
  ProjectorViewerFrustum f{};
  inPlace(f);
  f.inputs.file.value = base.toStdString();
  f.inputs.viewer.value = {0.f, 1.f, 0.f};
  f.inputs.near_plane.value = 0.5f;
  f.inputs.far_plane.value = 20.f;
  f.inputs.compute.value = halp::impulse_type{};
  run(f);
  INFO(f.outputs.status.value);
  REQUIRE(f.outputs.done.value);

  const auto pm = Threedim::parseMatrix(f.outputs.projection_rows.value);
  REQUIRE(pm);
  CHECK(*pm == f.outputs.projection.value);
  ossia::camera_component cam;
  std::copy_n(pm->begin(), 16, cam.projection_matrix);
  cam.projection_matrix_set = true;
  QMatrix4x4 P;
  score::gfx::cameraProjection(P, cam, 1.f);

  auto ndc = [&](QVector3D eye) {
    const QVector4D c = P * QVector4D(eye, 1.f);
    return QVector3D(c.x() / c.w(), c.y() / c.w(), c.z() / c.w());
  };
  const auto& fr = f.outputs.frustum.value;
  const QVector3D nearLL = ndc({fr[0] * 0.5f, fr[2] * 0.5f, -0.5f});
  const QVector3D farUR = ndc({fr[1] * 20.f, fr[3] * 20.f, -20.f});
  CHECK(nearLL.z() == Approx(1.f).margin(1e-4));
  CHECK(farUR.z() == Approx(-1.f).margin(1e-4));
  CHECK(nearLL.x() == Approx(-1.f).margin(1e-4));
  CHECK(nearLL.y() == Approx(-1.f).margin(1e-4));
  CHECK(farUR.x() == Approx(1.f).margin(1e-4));
  CHECK(farUR.y() == Approx(1.f).margin(1e-4));

  const auto pose = Threedim::parseMatrix(f.outputs.pose_rows.value);
  REQUIRE(pose);
  const QMatrix4x4 world{pose->data()};
  const QMatrix4x4 worldT = world.transposed();
  QMatrix4x4 view;
  std::copy_n(f.outputs.view.value.data(), 16, view.data());
  const QVector3D eye = worldT.map(QVector3D(0.f, 0.f, 0.f));
  CHECK(eye.distanceToPoint(QVector3D(0.f, 1.f, 0.f)) < 1e-5f);
  const QVector3D wallLL(
      f.outputs.pa.value.x, f.outputs.pa.value.y, f.outputs.pa.value.z);
  const QVector3D inEye = view.map(wallLL);
  CHECK(ndc(inEye).x() == Approx(-1.f).margin(1e-3));
  CHECK(ndc(inEye).y() == Approx(-1.f).margin(1e-3));
}

TEST_CASE("Projector to Viewer Frustum reads its XYZ map input once", "[offaxis][viewer]")
{
  auto c = wall({}, 32, 24);
  ProjectorViewerFrustum f{};
  inPlace(f);
  f.inputs.viewer.value = {0.f, 1.f, 0.f};
  f.inputs.compute.value = halp::impulse_type{};
  f();
  f.inputs.compute.value.reset();
  CHECK(f.inputs.xyz.request_readback);

  SECTION("the map arrives")
  {
    f.inputs.xyz.texture.bytes = c.xyz.data();
    f.inputs.xyz.texture.width = 32;
    f.inputs.xyz.texture.height = 24;
    run(f);
    INFO(f.outputs.status.value);
    CHECK_FALSE(f.inputs.xyz.request_readback);
    CHECK(f.outputs.status.value.starts_with("Done"));
  }
  SECTION("a resampled map is refused")
  {
    f.inputs.xyz.texture.bytes = c.xyz.data();
    f.inputs.xyz.texture.width = 32;
    f.inputs.xyz.texture.height = 24;
    f.inputs.xyz.resampled = true;
    run(f);
    CHECK(f.outputs.status.value.find("RGBA32F") != std::string::npos);
  }
  SECTION("nothing arrives")
  {
    for(int i = 0; i < ProjectorViewerFrustum::readbackFrames + 1; i++)
      f();
    CHECK_FALSE(f.inputs.xyz.request_readback);
    CHECK(f.outputs.status.value == "Failed: no XYZ map on the input");
  }
}

TEST_CASE("Viewer from pose settles on a still input", "[viewer]")
{
  Threedim::ViewerFromPose v;
  v.prepare({.rate = 48000.});
  v.inputs.scale.value = 1.f;
  v.inputs.min_cutoff.value = 1.f;
  v.inputs.beta.value = 0.f;
  v.inputs.d_cutoff.value = 1.f;
  v.inputs.position.value = {0.f, 0.f, 0.f};
  v(512);
  v.inputs.position.value = {1.f, 0.f, 0.f};
  v(512);
  const float first = v.outputs.viewer.value.x;
  CHECK(first < 0.5f);
  for(int i = 0; i < 48000 / 512 * 5; i++)
    v(512);
  CHECK(v.outputs.viewer.value.x == Approx(1.f).margin(1e-3));
}

TEST_CASE("Viewer from pose: transform, then the 1 euro filter", "[viewer]")
{
  constexpr int frames = 800;
  Threedim::ViewerFromPose v;
  v.prepare({.rate = 48000.});
  v.inputs.scale.value = 0.001f;
  v.inputs.transform.value = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.5f, 0, 0, 1};
  v.inputs.offset.value = {0.f, 0.1f, 0.f};
  v.inputs.min_cutoff.value = 1.f;
  v.inputs.beta.value = 0.f;
  v.inputs.d_cutoff.value = 1.f;
  std::mt19937 rng{3};
  std::normal_distribution<float> jitter(0.f, 5.f);
  double rawVar = 0., outVar = 0.;
  int n = 0;
  for(int i = 0; i < 600; i++)
  {
    v.inputs.position.value = {1000.f + jitter(rng), 1600.f + jitter(rng), 0.f};
    v(frames);
    if(i > 120)
    {
      rawVar += std::pow(v.outputs.raw.value.x - 1.5, 2);
      outVar += std::pow(v.outputs.viewer.value.x - 1.5, 2);
      n++;
    }
  }
  CHECK(v.outputs.raw.value.y == Approx(1.7f).margin(0.03f));
  CHECK(std::sqrt(outVar / n) < 0.3 * std::sqrt(rawVar / n));

  auto lagAfterStep = [](float beta) {
    Threedim::ViewerFromPose f;
    f.prepare({.rate = 48000.});
    f.inputs.scale.value = 1.f;
    f.inputs.min_cutoff.value = 1.f;
    f.inputs.beta.value = beta;
    f.inputs.d_cutoff.value = 1.f;
    for(int i = 0; i < 60; i++)
    {
      f.inputs.position.value = {0.f, 0.f, 0.f};
      f(frames);
    }
    for(int i = 0; i < 6; i++)
    {
      f.inputs.position.value = {float(0.2 * (i + 1) / 6.), 0.f, 0.f};
      f(frames);
    }
    return f.outputs.raw.value.x - f.outputs.viewer.value.x;
  };
  const float lag0 = lagAfterStep(0.f), lag1 = lagAfterStep(4.f);
  CHECK(lag1 < 0.6f * lag0);
}

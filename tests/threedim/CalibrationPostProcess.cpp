#include <Threedim/CalibrationFiles.hpp>
#include <Threedim/CalibrationPostProcess.hpp>
#include <Threedim/ProjectorBlend.hpp>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace Threedim;
using PP = CalibrationPostProcess;

namespace
{
constexpr int W = 48, H = 32;

//! A viewer camera's map of a projector: directions from the camera at the
//! origin, centred on `centre`, about 40 by 27 degrees.
std::shared_ptr<const StructuredLightCore::Result> viewerResult(std::array<double, 3> centre)
{
  auto r = std::make_shared<StructuredLightCore::Result>();
  r->ok = true;
  r->mode = StructuredLightCore::ViewerCamera;
  r->width = W;
  r->height = H;
  r->camWidth = 640;
  r->camHeight = 480;
  r->cameraIntrinsics = {500.f, 500.f, 320.f, 240.f};
  r->status = "decoded";
  const double l = std::sqrt(centre[0] * centre[0] + centre[1] * centre[1] + centre[2] * centre[2]);
  for(auto& c : centre)
    c /= l;
  const std::array<double, 3> up0{0, 1, 0};
  std::array<double, 3> right{
      centre[1] * up0[2] - centre[2] * up0[1], centre[2] * up0[0] - centre[0] * up0[2],
      centre[0] * up0[1] - centre[1] * up0[0]};
  double rl = std::sqrt(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
  if(rl < 1e-6)
  {
    right = {1, 0, 0};
    rl = 1;
  }
  for(auto& c : right)
    c /= rl;
  const std::array<double, 3> up{
      right[1] * centre[2] - right[2] * centre[1], right[2] * centre[0] - right[0] * centre[2],
      right[0] * centre[1] - right[1] * centre[0]};
  r->xyz.assign(std::size_t(W) * H * 4, 0.f);
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      const double u = (x + 0.5 - W / 2.) / 65., v = (y + 0.5 - H / 2.) / 65.;
      std::array<double, 3> d;
      for(int c = 0; c < 3; c++)
        d[c] = centre[c] + u * right[c] - v * up[c];
      const double n = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
      float* q = &r->xyz[(std::size_t(y) * W + x) * 4];
      for(int c = 0; c < 3; c++)
        q[c] = float(d[c] / n);
      q[3] = 1.f;
    }
  return r;
}

std::shared_ptr<const StructuredLightCore::Result> depthResult(float x0)
{
  auto r = std::make_shared<StructuredLightCore::Result>();
  r->ok = true;
  r->mode = StructuredLightCore::DepthCamera;
  r->width = W;
  r->height = H;
  r->status = "decoded";
  r->xyz.assign(std::size_t(W) * H * 4, 0.f);
  for(int y = 0; y < H; y++)
    for(int x = 0; x < W; x++)
    {
      float* q = &r->xyz[(std::size_t(y) * W + x) * 4];
      q[0] = x0 + x * 0.02f;
      q[1] = 1.f - y * 0.02f;
      q[2] = -2.f;
      q[3] = 1.f;
    }
  return r;
}

QStringList listed(const QString& base)
{
  QStringList out;
  for(const auto& f : readJsonFile(base + ".json").value("files").toArray())
    out.push_back(f.toString());
  return out;
}
}

TEST_CASE("The post-capture pipeline saves the results and their display", "[threedim][calibration][post]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  PP::Options o;
  o.names = {dir.filePath("set/room"), dir.filePath("set/room_p1")};
  o.outputs = {0, 1};
  o.screens = true;
  o.blend = true;
  const auto out = PP::run({depthResult(-0.5f), depthResult(0.3f)}, o, true);
  INFO(out.log.join('\n').toStdString());
  REQUIRE(out.ok);
  for(const auto& n : o.names)
  {
    const auto c = loadCalibration(n + ".json");
    REQUIRE(c.ok());
    CHECK(c.screen);
    CHECK(c.json.value("display").toString() == "screen");
    CHECK(listed(n).contains(QFileInfo{n}.fileName() + ".points.pfm"));
    CHECK_FALSE(QFile::exists(n + ".blend.json"));
  }
  CHECK_FALSE(out.log.join('\n').contains("Projector Blend Maps"));
}

TEST_CASE("Projector Blend Maps leaves the screens out", "[threedim][calibration][post]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  PP::Options o;
  o.names = {dir.filePath("wall")};
  o.screens = true;
  QStringList log;
  REQUIRE(PP::saveResults({depthResult(0.f)}, o, log));
  ProjectorBlendMaps::Job job;
  job.paths = {dir.filePath("wall.json").toStdString()};
  const auto r = ProjectorBlendMaps::run(job);
  INFO(r.status);
  CHECK_FALSE(r.ok);
  CHECK(r.status.find("1 screen(s) left out") != std::string::npos);
}

TEST_CASE("A recompute takes out the products it no longer makes", "[threedim][calibration][post]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  PP::Options o;
  o.names = {dir.filePath("front")};
  o.viewer = true;
  o.geometry = PP::Geometry::Box;
  o.box = {3.f, 3.f, 3.f, 3.f, 1.5f, 2.f};
  o.projection = Gfx::CanvasProjection::Equirectangular;
  const auto first = PP::run({viewerResult({0, 0, -1})}, o, true);
  INFO(first.log.join('\n').toStdString());
  REQUIRE(first.ok);
  const QString base = o.names[0];
  CHECK(listed(base).contains("front.points.pfm"));
  CHECK(listed(base).contains("front.warp.pfm"));
  CHECK(QFile::exists(base + ".measured.xyz.pfm"));
  CHECK(QFile::exists(base + ".measured.json"));

  QStringList log;
  const auto saved = PP::load(o.names, log);
  REQUIRE(saved.size() == 1);
  REQUIRE(saved[0]);
  o.geometry = PP::Geometry::Directions;
  o.projection = Gfx::CanvasProjection::None;
  const auto again = PP::run(saved, o, false);
  INFO(again.log.join('\n').toStdString());
  REQUIRE(again.ok);
  CHECK_FALSE(listed(base).contains("front.points.pfm"));
  CHECK_FALSE(QFile::exists(base + ".points.pfm"));
  CHECK_FALSE(listed(base).contains("front.warp.pfm"));
  CHECK_FALSE(QFile::exists(base + ".warp.pfm"));
  CHECK(loadCalibration(base + ".json").ok());
}

TEST_CASE("A floor output gets a points map, not a warp round the pole", "[threedim][calibration][post]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  PP::Options o;
  o.names = {dir.filePath("floor")};
  o.viewer = true;
  o.geometry = PP::Geometry::Box;
  o.box = {3.f, 3.f, 3.f, 3.f, 1.5f, 2.f};
  o.projection = Gfx::CanvasProjection::Equirectangular;
  o.cameraRotation = {-90.f, 0.f, 0.f};
  const auto out = PP::run({viewerResult({0, 0, -1})}, o, true);
  const QString log = out.log.join('\n');
  INFO(log.toStdString());
  REQUIRE(out.ok);
  CHECK(log.contains("pole"));
  CHECK(listed(o.names[0]).contains("floor.points.pfm"));
  CHECK_FALSE(QFile::exists(o.names[0] + ".warp.pfm"));
}

TEST_CASE("A cancelled post-capture pipeline stops before the maps", "[threedim][calibration][post]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  PP::Options o;
  o.names = {dir.filePath("front")};
  o.viewer = true;
  o.geometry = PP::Geometry::Box;
  o.box = {3.f, 3.f, 3.f, 3.f, 1.5f, 2.f};
  std::atomic_bool cancel{true};
  const auto out = PP::run({viewerResult({0, 0, -1})}, o, true, &cancel);
  CHECK_FALSE(out.ok);
  CHECK(out.status == "Aborted");
  CHECK_FALSE(QFile::exists(o.names[0] + ".points.pfm"));
}

TEST_CASE("A turned viewer camera is aligned to an earlier run", "[threedim][calibration][post]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  PP::Options o;
  o.names = {dir.filePath("earlier")};
  o.viewer = true;
  REQUIRE(PP::run({viewerResult({0, 0, -1})}, o, true).ok);

  o.names = {dir.filePath("now")};
  o.anchors = {{0, dir.filePath("earlier.json")}};
  const auto out = PP::run({viewerResult({0.2, 0, -1})}, o, true);
  const QString log = out.log.join('\n');
  INFO(log.toStdString());
  REQUIRE(out.ok);
  CHECK(log.contains("Aligned to the earlier calibration of output(s) 1"));
}

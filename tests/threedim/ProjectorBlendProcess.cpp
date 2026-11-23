// Projector Blend Maps as a process: calibrations saved the way Structured Light
// Calibration saves them, blended, and the PFM maps read back.

#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Gfx/Graph/FloatImageIO.hpp>
#include <Threedim/CalibrationFiles.hpp>
#include <Threedim/ProjectorBlend.hpp>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace procam = score::gfx::procam;
using Threedim::CalibrationFile;
using Threedim::ProjectorBlendMaps;

namespace
{
constexpr int PW = 200, PH = 120;

CalibrationFile wallProjector(double x, const QString& base)
{
  procam::Model m;
  m.fx = m.fy = 170.;
  m.cx = 99.5;
  m.cy = 59.5;
  const Eigen::Matrix3d flip = Eigen::Vector3d(1., -1., -1.).asDiagonal();
  m.R = flip.transpose();
  m.t = -m.R * Eigen::Vector3d(x, 1.2, 0.);
  CalibrationFile c;
  c.base = base;
  c.width = PW;
  c.height = PH;
  c.xyz.assign(PW * PH * 4, 0.f);
  const Eigen::Matrix3d Rt = m.R.transpose();
  for(int v = 0; v < PH; v++)
    for(int u = 0; u < PW; u++)
    {
      const Eigen::Vector3d d = Rt * Eigen::Vector3d((u - m.cx) / m.fx, (v - m.cy) / m.fy, 1.);
      const Eigen::Vector3d p = m.position() + (-2. / d.z()) * d;
      float* q = c.xyz.data() + (v * PW + u) * 4;
      q[0] = float(p.x());
      q[1] = float(p.y());
      q[2] = float(p.z());
      q[3] = 1.f;
    }
  c.intrinsics = {float(m.fx), float(m.fy), float(m.cx), float(m.cy)};
  c.pose = procam::toColumnMajor(m.worldFromLocal());
  c.hasPose = true;
  return c;
}

template <typename T>
void inPlace(T& obj, int* requests = nullptr)
{
  obj.worker.request = [requests](auto jobs, uint64_t id, auto job) {
    if(requests)
      ++*requests;
    T::worker::work(std::move(jobs), id, std::move(job));
  };
}

void press(ProjectorBlendMaps& obj)
{
  obj.inputs.compute.value = halp::impulse_type{};
  obj();
  obj.inputs.compute.value.reset();
  for(int i = 0; i < ProjectorBlendMaps::readbackFrames + 1; i++)
    obj();
}

void setTexture(Threedim::FloatMapInput<"XYZ map 1">& in, CalibrationFile& c)
{
  in.texture.bytes = c.xyz.data();
  in.texture.width = c.width;
  in.texture.height = c.height;
}
}

TEST_CASE("Projector Blend Maps writes alpha and beta PFMs for each calibration", "[blend]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString a = dir.filePath("left");
  const QString b = dir.filePath("right");
  REQUIRE(Threedim::saveCalibration(wallProjector(-1.0, a)));
  REQUIRE(Threedim::saveCalibration(wallProjector(1.0, b)));

  const auto la = Threedim::loadCalibration(a + ".xyz.pfm");
  REQUIRE(la.ok());
  CHECK(la.hasModel());

  ProjectorBlendMaps obj{};
  inPlace(obj);
  obj.inputs.files.value = QString(a + ".xyz.pfm\n" + b + ".json").toStdString();
  obj.inputs.black_level.value = 0.03f;
  bool done = false;
  obj.inputs.compute.value = halp::impulse_type{};
  for(int i = 0; i < 10 && !done; i++)
  {
    obj();
    obj.inputs.compute.value.reset();
    done = bool(obj.outputs.done.value);
  }
  INFO(obj.outputs.status.value);
  REQUIRE(done);
  REQUIRE(obj.outputs.alpha_files.value.size() == 2);
  CHECK(obj.outputs.overlap.value[0] > 0.1f);

  const auto alphaL = score::gfx::readPFM(a + ".alpha.pfm");
  const auto alphaR = score::gfx::readPFM(b + ".alpha.pfm");
  const auto betaL = score::gfx::readPFM(a + ".beta.pfm");
  const auto betaR = score::gfx::readPFM(b + ".beta.pfm");
  REQUIRE(alphaL.valid());
  REQUIRE(alphaR.valid());
  REQUIRE(betaL.valid());
  REQUIRE(betaR.valid());
  CHECK(alphaL.channels == 1);
  CHECK(alphaL.width == PW);

  // Columns at the same wall x: left u lights x = -1 + (u - cx) * 2 / fx.
  double worst = 0.;
  for(int u = 0; u < PW; u++)
  {
    const int ur = u - int(std::lround(2. * 170. / 2.));
    if(ur < 0)
      continue;
    const int v = PH / 2;
    const double s = alphaL.data[v * PW + u] + alphaR.data[v * PW + ur];
    worst = std::max(worst, std::abs(s - 1.));
  }
  INFO("worst |alpha_L + alpha_R - 1| on the overlap " << worst);
  CHECK(worst < 0.02);
  CHECK(obj.outputs.alpha1.texture.width == PW);

  QFile f{a + ".blend.json"};
  REQUIRE(f.open(QIODevice::ReadOnly));
  const auto o = QJsonDocument::fromJson(f.readAll()).object();
  CHECK(o["projectorCount"].toInt() == 2);
  CHECK(o["reprojection"].toString() == "pose");
}

TEST_CASE("Projector Blend Maps computes once for every renderer of its node", "[blend]")
{
  QTemporaryDir dir;
  const QString a = dir.filePath("left"), b = dir.filePath("right");
  REQUIRE(Threedim::saveCalibration(wallProjector(-1.0, a)));
  REQUIRE(Threedim::saveCalibration(wallProjector(1.0, b)));
  auto shared = std::make_shared<ProjectorBlendMaps::Jobs>();
  ProjectorBlendMaps first{}, second{};
  int requests = 0;
  for(auto* o : {&first, &second})
  {
    o->node_state = shared;
    inPlace(*o, &requests);
    o->inputs.files.value = QString(a + "\n" + b).toStdString();
    o->inputs.compute.value = halp::impulse_type{};
  }
  for(int i = 0; i < ProjectorBlendMaps::readbackFrames + 2; i++)
    for(auto* o : {&first, &second})
    {
      (*o)();
      o->inputs.compute.value.reset();
    }
  CHECK(requests == 1);
  CHECK(first.outputs.alpha1.texture.width == PW);
  CHECK(second.outputs.alpha1.texture.width == PW);
  CHECK(second.outputs.status.value.starts_with("Done"));
}

TEST_CASE("Projector Blend Maps names the maps of its texture inputs as a set", "[blend]")
{
  QTemporaryDir dir;
  auto left = wallProjector(-1.0, {});
  ProjectorBlendMaps obj{};
  inPlace(obj);
  setTexture(obj.inputs.xyz1, left);
  obj.inputs.prefix.value = dir.filePath("wall.json").toStdString();

  SECTION("written as wall, the first of the set")
  {
    press(obj);
    INFO(obj.outputs.status.value);
    REQUIRE(obj.outputs.alpha_files.value.size() == 1);
    CHECK(obj.outputs.alpha_files.value[0] == dir.filePath("wall.alpha.pfm").toStdString());
    CHECK(Threedim::readJsonFile(dir.filePath("wall.blend.json"))["alphaMap"].toString()
          == "wall.alpha.pfm");
  }
  SECTION("a calibration of that name and another size is not written over")
  {
    auto other = wallProjector(-1.0, dir.filePath("wall"));
    other.width = 10;
    other.height = 6;
    other.xyz.assign(10 * 6 * 4, 1.f);
    REQUIRE(Threedim::saveCalibration(other));
    press(obj);
    INFO(obj.outputs.status.value);
    CHECK(obj.outputs.status.value.find("not written") != std::string::npos);
    CHECK(obj.outputs.alpha_files.value[0].empty());
    CHECK_FALSE(QFile::exists(dir.filePath("wall.alpha.pfm")));
  }
  SECTION("a resampled input is refused")
  {
    obj.inputs.xyz1.resampled = true;
    press(obj);
    CHECK(obj.outputs.status.value.find("RGBA32F") != std::string::npos);
    CHECK_FALSE(QFile::exists(dir.filePath("wall.alpha.pfm")));
  }
}

TEST_CASE("Projector Blend Maps refuses settings that are not numbers", "[blend]")
{
  ProjectorBlendMaps obj{};
  int requests = 0;
  inPlace(obj, &requests);
  auto left = wallProjector(-1.0, {});
  setTexture(obj.inputs.xyz1, left);
  obj.inputs.ramp.value = "10, nan";
  press(obj);
  CHECK(requests == 0);
  CHECK(obj.outputs.status.value.starts_with("Failed: Ramp widths"));
  obj.inputs.ramp.value = "10";
  obj.inputs.exponent.value = "0";
  press(obj);
  CHECK(requests == 0);
  CHECK(obj.outputs.status.value.starts_with("Failed: Blend exponents"));
}

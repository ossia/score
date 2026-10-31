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

#include <chrono>
#include <cmath>
#include <thread>

namespace procam = score::gfx::procam;
using Threedim::CalibrationFile;
using Threedim::ProjectorBlendMaps;

namespace
{
constexpr int PW = 200, PH = 120;

CalibrationFile wallProjector(double x, const std::string& base)
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
}

TEST_CASE("Projector Blend Maps writes alpha and beta PFMs for each calibration", "[blend]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const std::string a = dir.filePath("left").toStdString();
  const std::string b = dir.filePath("right").toStdString();
  REQUIRE(Threedim::saveCalibration(wallProjector(-1.0, a)));
  REQUIRE(Threedim::saveCalibration(wallProjector(1.0, b)));

  const auto la = Threedim::loadCalibration(a + ".xyz.pfm");
  REQUIRE(la.ok());
  CHECK(la.hasModel());

  ProjectorBlendMaps obj;
  for(auto* t : {&obj.inputs.xyz1.texture, &obj.inputs.xyz2.texture, &obj.inputs.xyz3.texture,
                 &obj.inputs.xyz4.texture})
  {
    t->bytes = nullptr;
    t->width = t->height = 0;
  }
  obj.inputs.files.value = a + ".xyz.pfm\n" + b + ".json";
  obj.inputs.black_level.value = 0.03f;
  obj.inputs.compute.value = halp::impulse_type{};
  bool done = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
  while(!done && std::chrono::steady_clock::now() < deadline)
  {
    obj();
    obj.inputs.compute.value.reset();
    done = bool(obj.outputs.done.value);
    if(!done)
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  INFO(obj.outputs.status.value);
  REQUIRE(done);
  REQUIRE(obj.outputs.alpha_files.value.size() == 2);
  CHECK(obj.outputs.overlap.value[0] > 0.1f);

  const auto alphaL = score::gfx::readPFM(QString::fromStdString(a + ".alpha.pfm"));
  const auto alphaR = score::gfx::readPFM(QString::fromStdString(b + ".alpha.pfm"));
  const auto betaL = score::gfx::readPFM(QString::fromStdString(a + ".beta.pfm"));
  const auto betaR = score::gfx::readPFM(QString::fromStdString(b + ".beta.pfm"));
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

  QFile f{QString::fromStdString(a + ".blend.json")};
  REQUIRE(f.open(QIODevice::ReadOnly));
  const auto o = QJsonDocument::fromJson(f.readAll()).object();
  CHECK(o["projectorCount"].toInt() == 2);
  CHECK(o["reprojection"].toString() == "pose");
}

// Manual Projector Calibration: a synthetic projector recovered from 4 to 12
// noisy 2D <-> 3D pairs, the file round trip and the process itself.

#include <Gfx/Calibration/EPnP.hpp>
#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Threedim/CalibrationFiles.hpp>
#include <Threedim/ManualProjectorCalibration.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numeric>
#include <random>

namespace procam = score::gfx::procam;
using Catch::Approx;
using Threedim::ManualProjectorCalibration;
using Pair = ManualProjectorCalibration::Pair;

namespace
{
constexpr int W = 1920, H = 1080;

procam::Model projector()
{
  procam::Model m;
  m.fx = 1050.;
  m.fy = 1055.;
  m.cx = 975.;
  m.cy = 530.;
  const Eigen::Vector3d eye(0.4, 1.7, 3.2);
  const Eigen::Vector3d target(-0.2, 1.2, -1.0);
  const Eigen::Vector3d z = (target - eye).normalized();
  const Eigen::Vector3d x = z.cross(Eigen::Vector3d::UnitY()).normalized();
  const Eigen::Vector3d y = z.cross(x);
  Eigen::Matrix3d Rt;
  Rt << x, y, z;
  m.R = Rt.transpose();
  m.t = -m.R * eye;
  return m;
}

//! Points on a room corner: back wall z = -1, left wall x = -1.5, floor y = 0.
std::vector<Eigen::Vector3d> venuePoints()
{
  return {{-1.0, 0.5, -1.0}, {0.5, 2.0, -1.0},  {1.2, 0.8, -1.0},  {-0.3, 1.5, -1.0},
          {-1.5, 0.6, -0.4}, {-1.5, 1.9, 0.3},  {-1.5, 1.1, 0.9},  {0.6, 0.0, 0.4},
          {-0.8, 0.0, 0.8},  {0.9, 2.2, -1.0},  {-1.5, 2.3, -0.8}, {0.1, 0.0, -0.3}};
}

std::vector<Pair> observe(const procam::Model& m, int n, double noise, unsigned seed)
{
  std::mt19937 rng(seed);
  std::normal_distribution<double> g(0., std::max(noise, 1e-12));
  std::vector<Pair> out;
  const auto pts = venuePoints();
  for(int i = 0; i < n; i++)
  {
    procam::Vec2 px;
    REQUIRE(m.project(pts[i], px));
    REQUIRE(px.x() > 0);
    REQUIRE(px.x() < W);
    REQUIRE(px.y() > 0);
    REQUIRE(px.y() < H);
    out.push_back(
        {float(px.x() + g(rng)), float(px.y() + g(rng)),
         {float(pts[i].x()), float(pts[i].y()), float(pts[i].z())}});
  }
  return out;
}

Eigen::Vector3d position(const ManualProjectorCalibration::Solution& s)
{
  return procam::fromColumnMajor(s.pose).topRightCorner<3, 1>();
}
}

TEST_CASE("Manual calibration recovers a projector from 6 to 12 noisy points", "[manualcal]")
{
  const auto truth = projector();
  ManualProjectorCalibration::Settings set;
  set.width = W;
  set.height = H;
  for(int n : {6, 8, 12})
  {
    INFO(n << " points");
    const auto pairs = observe(truth, n, n == 6 ? 0.2 : 0.5, 17u + n);
    const auto sol = ManualProjectorCalibration::solve(pairs, set);
    REQUIRE(sol.ok);
    CHECK(sol.rms < 1.f);
    CHECK(sol.errors.size() == pairs.size());
    const double tol = n == 6 ? 0.04 : 0.02;
    CHECK(sol.intrinsics[0] == Approx(truth.fx).epsilon(tol));
    CHECK(sol.intrinsics[1] == Approx(truth.fy).epsilon(tol));
    CHECK(std::abs(sol.intrinsics[2] - truth.cx) < 40.);
    CHECK(std::abs(sol.intrinsics[3] - truth.cy) < 40.);
    CHECK((position(sol) - truth.position()).norm() < (n == 6 ? 0.1 : 0.05));
  }
}

TEST_CASE("Manual calibration with locked intrinsics needs 4 points", "[manualcal]")
{
  const auto truth = projector();
  ManualProjectorCalibration::Settings set;
  set.width = W;
  set.height = H;
  set.intrinsicsMode = ManualProjectorCalibration::LockIntrinsics;
  set.lockedK = {float(truth.fx), float(truth.fy), float(truth.cx), float(truth.cy)};

  SECTION("4 and 5 exact points off a plane: EPnP")
  {
    for(int n : {4, 5})
    {
      const auto sol = ManualProjectorCalibration::solve(observe(truth, n, 0., 3u), set);
      REQUIRE(sol.ok);
      CHECK(sol.intrinsicsLocked);
      CHECK(sol.rms < 0.05f);
      CHECK((position(sol) - truth.position()).norm() < 1e-3);
    }
  }
  SECTION("7 noisy points")
  {
    const auto sol = ManualProjectorCalibration::solve(observe(truth, 7, 0.5, 5u), set);
    REQUIRE(sol.ok);
    CHECK(sol.rms < 1.f);
    CHECK((position(sol) - truth.position()).norm() < 0.03);
  }
  SECTION("4 coplanar points")
  {
    std::vector<Pair> pairs;
    for(auto X : std::vector<Eigen::Vector3d>{
            {-1.0, 0.5, -1.0}, {0.5, 2.0, -1.0}, {1.2, 0.8, -1.0}, {-0.3, 1.5, -1.0}})
    {
      procam::Vec2 px;
      REQUIRE(truth.project(X, px));
      pairs.push_back({float(px.x()), float(px.y()), {float(X.x()), float(X.y()), float(X.z())}});
    }
    const auto sol = ManualProjectorCalibration::solve(pairs, set);
    REQUIRE(sol.ok);
    CHECK((position(sol) - truth.position()).norm() < 1e-2);
  }
  SECTION("Throw ratio and lens shift give the locked intrinsics")
  {
    procam::Model m = truth;
    m.fx = m.fy = 1.2 * W;
    m.cx = (W - 1) * 0.5 - 0.1 * W;
    m.cy = (H - 1) * 0.5 + 0.2 * H;
    auto s2 = set;
    s2.lockedK = {};
    s2.throwRatio = 1.2f;
    s2.lensShift = {0.1f, 0.2f};
    std::vector<Pair> pairs;
    for(const auto& X : venuePoints())
    {
      procam::Vec2 px;
      if(m.project(X, px) && px.x() > 0 && px.x() < W && px.y() > 0 && px.y() < H)
        pairs.push_back({float(px.x()), float(px.y()), {float(X.x()), float(X.y()), float(X.z())}});
    }
    REQUIRE(pairs.size() >= 4);
    const auto sol = ManualProjectorCalibration::solve(pairs, s2);
    REQUIRE(sol.ok);
    CHECK(sol.intrinsics[0] == Approx(1.2f * W));
    CHECK(sol.intrinsics[2] == Approx(m.cx));
    CHECK((position(sol) - truth.position()).norm() < 1e-3);
  }
  SECTION("Too few points")
  {
    auto s2 = set;
    s2.intrinsicsMode = ManualProjectorCalibration::SolveIntrinsics;
    CHECK_FALSE(ManualProjectorCalibration::solve(observe(truth, 5, 0., 1u), s2).ok);
    CHECK_FALSE(ManualProjectorCalibration::solve(observe(truth, 3, 0., 1u), set).ok);
  }
}

TEST_CASE("EPnP alone on 4 to 12 random points", "[manualcal][epnp]")
{
  const auto truth = projector();
  std::mt19937 rng(42);
  std::uniform_real_distribution<double> ux(-1.5, 1.5), uy(0., 2.5), uz(-1., 1.);
  for(int n : {4, 6, 12})
  {
    std::vector<procam::Vec3> X;
    std::vector<procam::Vec2> xn;
    while((int)X.size() < n)
    {
      const procam::Vec3 p(ux(rng), uy(rng), uz(rng));
      const procam::Vec3 c = truth.R * p + truth.t;
      if(c.z() < 0.5)
        continue;
      X.push_back(p);
      xn.emplace_back(c.x() / c.z(), c.y() / c.z());
    }
    std::vector<int> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    procam::Mat3 R;
    procam::Vec3 t;
    REQUIRE(procam::epnp::solve(X, xn, idx, R, t));
    CHECK((R - truth.R).norm() < 1e-6);
    CHECK((t - truth.t).norm() < 1e-6);
  }
}

TEST_CASE("Manual calibration file round trip", "[manualcal]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto truth = projector();
  ManualProjectorCalibration::Settings set;
  set.width = 320;
  set.height = 180;
  procam::Model small = truth;
  small.fx /= 6.;
  small.fy /= 6.;
  small.cx /= 6.;
  small.cy /= 6.;
  std::vector<Pair> pairs;
  for(const auto& X : venuePoints())
  {
    procam::Vec2 px;
    REQUIRE(small.project(X, px));
    pairs.push_back({float(px.x()), float(px.y()), {float(X.x()), float(X.y()), float(X.z())}});
  }
  const auto sol = ManualProjectorCalibration::solve(pairs, set);
  REQUIRE(sol.ok);
  const std::vector<std::array<float, 4>> planes{
      {0.f, 0.f, 1.f, 1.f}, {1.f, 0.f, 0.f, 1.5f}, {0.f, 1.f, 0.f, 0.f}};

  const std::string base = dir.filePath("manual").toStdString();
  REQUIRE(ManualProjectorCalibration::saveFile(base + ".json", pairs, sol, set, planes));

  QFile f{QString::fromStdString(base + ".json")};
  REQUIRE(f.open(QIODevice::ReadOnly));
  const auto o = QJsonDocument::fromJson(f.readAll()).object();
  CHECK(o["mode"].toString() == "manual");
  CHECK(o["points"].toArray().size() == int(pairs.size()));
  CHECK(o["rms"].toDouble() == Approx(sol.rms));

  const auto loaded = ManualProjectorCalibration::loadFile(base + ".json");
  REQUIRE(loaded.ok);
  CHECK(loaded.pairs.size() == pairs.size());
  CHECK(loaded.width == 320);
  CHECK(loaded.solution.ok);
  CHECK(loaded.solution.intrinsics[0] == Approx(sol.intrinsics[0]));

  const auto c = Threedim::loadCalibration(base + ".json");
  REQUIRE(c.ok());
  CHECK(c.hasModel());
  CHECK(c.width == 320);
  int valid = 0;
  double worst = 0.;
  for(int i = 0; i < c.width * c.height; i++)
  {
    if(c.xyz[i * 4 + 3] <= 0.f)
      continue;
    valid++;
    const procam::Vec3 p(c.xyz[i * 4], c.xyz[i * 4 + 1], c.xyz[i * 4 + 2]);
    double d = 1e9;
    for(const auto& pl : planes)
      d = std::min(d, std::abs(pl[0] * p.x() + pl[1] * p.y() + pl[2] * p.z() + pl[3]));
    worst = std::max(worst, d);
  }
  CHECK(valid > c.width * c.height * 9 / 10);
  CHECK(worst < 1e-3);

  ManualProjectorCalibration obj;
  obj.inputs.projector_size.value = {640, 360};
  halp::mmap_file_view view{};
  const std::string path = base + ".json";
  view.filename = path;
  ManualProjectorCalibration::ins::load_t::process(view)(obj);
  obj();
  INFO(obj.outputs.status.value);
  REQUIRE(obj.pairs().size() == pairs.size());
  CHECK(obj.pairs()[0].u == Approx((pairs[0].u + 0.5) * 2. - 0.5));
  CHECK(obj.pairs()[0].v == Approx((pairs[0].v + 0.5) * 2. - 0.5));
  REQUIRE(obj.solution().ok);
  CHECK(obj.solution().intrinsics[0] == Approx(2. * sol.intrinsics[0]).epsilon(1e-3));
}

TEST_CASE("Manual calibration process records pairs and draws them", "[manualcal]")
{
  ManualProjectorCalibration obj;
  obj.inputs.projector_size.value = {320, 180};
  obj.inputs.intrinsics_mode.value = ManualProjectorCalibration::LockIntrinsics;
  procam::Model m = projector();
  m.fx /= 6.;
  m.fy /= 6.;
  m.cx /= 6.;
  m.cy /= 6.;
  obj.inputs.locked_intrinsics.value = {float(m.fx), float(m.fy), float(m.cx), float(m.cy)};

  const auto pts = venuePoints();
  procam::Vec2 px4;
  for(int i = 0; i < 5; i++)
  {
    procam::Vec2 px;
    REQUIRE(m.project(pts[i], px));
    obj.inputs.crosshair.value
        = {float((px.x() + 0.5) / 320.), float(1. - (px.y() + 0.5) / 180.)};
    obj.inputs.world.value = {float(pts[i].x()), float(pts[i].y()), float(pts[i].z())};
    obj.inputs.record.value.emplace();
    obj();
    obj.inputs.record.value.reset();
    obj();
  }
  CHECK(obj.outputs.count.value == 5);
  {
    REQUIRE(m.project(pts[4], px4));
    CHECK(std::abs(obj.pairs().back().u - px4.x()) < 1e-3);
    CHECK(std::abs(obj.pairs().back().v - px4.y()) < 1e-3);
  }
  CHECK(obj.solution().ok);
  CHECK(obj.outputs.rms.value < 0.05f);
  CHECK((procam::fromColumnMajor(obj.outputs.pose.value).topRightCorner<3, 1>()
         - m.position())
            .norm()
        < 1e-3);

  const auto& tex = obj.outputs.pattern.texture;
  REQUIRE(tex.width == 320);
  REQUIRE(tex.height == 180);
  int lit = 0;
  for(int i = 0; i < 320 * 180; i++)
    lit += tex.bytes[i * 4 + 1] > 64;
  CHECK(lit > 50);

  obj.inputs.delete_last.value.emplace();
  obj();
  obj.inputs.delete_last.value.reset();
  CHECK(obj.outputs.count.value == 4);

  obj.inputs.points.value = "10 20 0 0 -1; 30 40 1 0 -1";
  obj();
  CHECK(obj.outputs.count.value == 6);
}

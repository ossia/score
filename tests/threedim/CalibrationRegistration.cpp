// Register Calibrations: a projector seen from two camera positions recovers the
// transform between their frames, with noise and outliers; the other
// calibrations of the second frame are moved into the first.

#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Gfx/Calibration/Registration.hpp>
#include <Threedim/CalibrationFiles.hpp>
#include <Threedim/CalibrationRegistration.hpp>

#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>

namespace reg = score::gfx::registration;
namespace procam = score::gfx::procam;
using Threedim::CalibrationFile;
using Threedim::RegisterCalibrations;

namespace
{
constexpr int PW = 160, PH = 100;

reg::Transform knownTransform()
{
  reg::Transform T;
  T.R = (Eigen::AngleAxisd(0.7, Eigen::Vector3d(0.2, 1., 0.1).normalized())).toRotationMatrix();
  T.t = Eigen::Vector3d(1.5, -0.3, 2.2);
  return T;
}

// A projector on a corner (wall z = -2, wall x = -1.5), in the reference frame.
CalibrationFile projectorIn(const reg::Transform& frameFromRef, double noise, double outliers,
                            const std::string& base, double x0 = 0.)
{
  procam::Model m;
  m.fx = m.fy = 150.;
  m.cx = 79.5;
  m.cy = 49.5;
  const Eigen::Matrix3d flip = Eigen::Vector3d(1., -1., -1.).asDiagonal();
  m.R = (Eigen::AngleAxisd(0.4, Eigen::Vector3d::UnitY()).toRotationMatrix() * flip).transpose();
  m.t = -m.R * Eigen::Vector3d(x0, 1.2, 0.);
  std::mt19937 rng{42};
  std::normal_distribution<double> n(0., std::max(noise, 1e-12));
  std::uniform_real_distribution<double> uni(0., 1.);
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
      const Eigen::Vector3d o = m.position();
      double t = 1e9;
      if(d.z() < 0.)
        t = (-2. - o.z()) / d.z();
      if(d.x() < 0.)
        t = std::min(t, (-1.5 - o.x()) / d.x());
      Eigen::Vector3d p = frameFromRef(o + t * d);
      p += Eigen::Vector3d(n(rng), n(rng), n(rng));
      if(uni(rng) < outliers)
        p += Eigen::Vector3d(uni(rng), uni(rng), uni(rng)) * 2.;
      float* q = c.xyz.data() + (v * PW + u) * 4;
      q[0] = float(p.x());
      q[1] = float(p.y());
      q[2] = float(p.z());
      q[3] = 1.f;
    }
  c.intrinsics = {float(m.fx), float(m.fy), float(m.cx), float(m.cy)};
  Eigen::Matrix4d W = m.worldFromLocal();
  Eigen::Matrix4d F = Eigen::Matrix4d::Identity();
  F.topLeftCorner<3, 3>() = frameFromRef.R;
  F.topRightCorner<3, 1>() = frameFromRef.t;
  c.pose = procam::toColumnMajor(F * W);
  c.hasPose = true;
  return c;
}
}

TEST_CASE("Umeyama recovers a similarity exactly", "[registration]")
{
  std::mt19937 rng{1};
  std::uniform_real_distribution<double> u(-1., 1.);
  reg::Transform T = knownTransform();
  T.scale = 1.3;
  std::vector<reg::Vec3> x, y;
  for(int i = 0; i < 50; i++)
  {
    x.emplace_back(u(rng), u(rng), u(rng));
    y.push_back(T(x.back()));
  }
  reg::Transform S;
  REQUIRE(reg::umeyama(x, y, S, true));
  CHECK((S.R - T.R).norm() < 1e-9);
  CHECK((S.t - T.t).norm() < 1e-9);
  CHECK(S.scale == Catch::Approx(1.3));
  reg::Transform rigid;
  REQUIRE(reg::umeyama(x, y, rigid, false));
  CHECK(rigid.scale == 1.);
  CHECK(rigid.R.determinant() == Catch::Approx(1.));
}

TEST_CASE("Register Calibrations: shared projector, noise and outliers", "[registration]")
{
  QTemporaryDir dir;
  const auto T = knownTransform();
  // Frame B = T^-1 (reference): the transform to recover is T.
  reg::Transform Tinv;
  Tinv.R = T.R.transpose();
  Tinv.t = -T.R.transpose() * T.t;
  const reg::Transform identity;

  const auto a = projectorIn(identity, 0.004, 0., dir.filePath("A_shared").toStdString());
  const auto b = projectorIn(Tinv, 0.004, 0.2, dir.filePath("B_shared").toStdString());
  const auto other = projectorIn(Tinv, 0., 0., dir.filePath("B_other").toStdString(), 0.8);
  const auto otherTruth = projectorIn(identity, 0., 0., "", 0.8);
  REQUIRE(Threedim::saveCalibration(a));
  REQUIRE(Threedim::saveCalibration(b));
  REQUIRE(Threedim::saveCalibration(other));

  RegisterCalibrations obj;
  obj.inputs.reference.value = a.base + ".xyz.pfm";
  obj.inputs.other.value = b.base + ".xyz.pfm";
  obj.inputs.move.value = other.base + ".json";
  obj.inputs.run.value = halp::impulse_type{};
  obj();
  INFO(obj.outputs.status.value);
  REQUIRE(obj.outputs.done.value);
  const Eigen::Matrix4d M = procam::fromColumnMajor(obj.outputs.transform.value);
  const double angErr
      = Eigen::AngleAxisd(Eigen::Matrix3d(M.topLeftCorner<3, 3>() * T.R.transpose())).angle();
  const double posErr = (Eigen::Vector3d(M.topRightCorner<3, 1>()) - T.t).norm();
  INFO("rotation error " << angErr * 180. / M_PI << " deg, translation error " << posErr
                         << " m, RMS " << obj.outputs.rms.value << ", inliers "
                         << obj.outputs.inliers.value);
  CHECK(angErr < 0.002);
  CHECK(posErr < 0.005);
  CHECK(obj.outputs.rms.value < 0.01f);
  REQUIRE(obj.outputs.written.value.size() == 1);

  const auto moved = Threedim::loadCalibration(obj.outputs.written.value[0]);
  REQUIRE(moved.ok());
  double maxErr = 0.;
  for(std::size_t i = 0; i < moved.xyz.size(); i += 4)
    for(int c = 0; c < 3; c++)
      maxErr = std::max(maxErr, double(std::abs(moved.xyz[i + c] - otherTruth.xyz[i + c])));
  CHECK(maxErr < 0.02);
  const auto P = procam::fromColumnMajor(moved.pose);
  const auto Q = procam::fromColumnMajor(otherTruth.pose);
  CHECK((P - Q).norm() < 0.01);
}

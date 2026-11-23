#pragma once
#include <score_plugin_gfx_export.h>
#include <Eigen/Geometry>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

// Generalized (off-axis) perspective projection of Kooima 2008, "Generalized
// Perspective Projection" (http://csc.lsu.edu/~kooima/pdfs/gen-perspective.pdf;
// also "Perspective Projection for VR", VR Developer Gems 2019,
// doi:10.1201/b21598-33): a screen rectangle given by its lower-left pa,
// lower-right pb and upper-left pc corners, seen from the eye pe.
// The tight viewer frustum of a projector's lit surface for two-pass rendering
// follows Raskar et al. 1998, "The Office of the Future" (doi:10.1145/280814.280861).
namespace score::gfx::offaxis
{
using Vec3 = Eigen::Vector3d;

struct Frustum
{
  //! Screen basis: right, up, normal (towards the eye).
  Vec3 vr{1., 0., 0.}, vu{0., 1., 0.}, vn{0., 0., 1.};
  //! Tangents at unit distance from the eye: left, right, bottom, top.
  double l{-1.}, r{1.}, b{-1.}, t{1.};
  //! Eye to screen plane distance.
  double d{1.};
  bool ok{};

  //! World from eye (OpenGL camera: -Z forward, +Y up), translation pe.
  Eigen::Matrix4d worldFromEye(const Vec3& pe) const noexcept
  {
    Eigen::Matrix4d m = Eigen::Matrix4d::Identity();
    m.block<3, 1>(0, 0) = vr;
    m.block<3, 1>(0, 1) = vu;
    m.block<3, 1>(0, 2) = vn;
    m.block<3, 1>(0, 3) = pe;
    return m;
  }
  //! Kooima's M^T T(-pe).
  Eigen::Matrix4d view(const Vec3& pe) const noexcept
  {
    Eigen::Matrix4d m = Eigen::Matrix4d::Identity();
    m.block<1, 3>(0, 0) = vr.transpose();
    m.block<1, 3>(1, 0) = vu.transpose();
    m.block<1, 3>(2, 0) = vn.transpose();
    m.block<3, 1>(0, 3) = -(m.topLeftCorner<3, 3>() * pe);
    return m;
  }
  //! The off-axis frustum in score's reverse-Z convention (as
  //! setReverseZPerspective): view z = -n gives NDC z = +1, z = -f gives -1.
  //! The identity when 0 < n < f does not hold or the frustum is empty.
  Eigen::Matrix4d projection(double n, double f) const noexcept
  {
    const double L = l * n, R = r * n, B = b * n, T = t * n;
    if(!(n > 0.) || !(f > n) || !(R > L) || !(T > B) || !std::isfinite(f))
      return Eigen::Matrix4d::Identity();
    Eigen::Matrix4d P = Eigen::Matrix4d::Zero();
    P(0, 0) = 2. * n / (R - L);
    P(0, 2) = (R + L) / (R - L);
    P(1, 1) = 2. * n / (T - B);
    P(1, 2) = (T + B) / (T - B);
    P(2, 2) = (f + n) / (f - n);
    P(2, 3) = 2. * f * n / (f - n);
    P(3, 2) = -1.;
    return P;
  }
};

SCORE_PLUGIN_GFX_EXPORT Frustum kooima(const Vec3& pa, const Vec3& pb, const Vec3& pc, const Vec3& pe);

struct ViewerFrustum
{
  bool ok{};
  std::string error;
  Frustum frustum;
  //! Screen corners on the fitted plane (planar surface) or at unit distance.
  Vec3 pa, pb, pc;
  bool planar{};
  double planeRms{};
  //! Render size whose pixels are no larger, seen from the viewer, than the
  //! given percentile of the projector's pixels.
  int width{}, height{};
  //! Angular size of the projector pixels (radians): median and percentile.
  double pixelMedian{}, pixelPercentile{};
};

//! The tight off-axis frustum, from the viewer, of every lit point of an XYZ
//! map (RGBA, rows top to bottom, w > 0 lit). A surface within planeTolerance
//! of a plane gets the screen on that plane, oriented with the world up.
SCORE_PLUGIN_GFX_EXPORT ViewerFrustum fitViewerFrustum(
    const std::vector<float>& xyz, int W, int H, const Vec3& pe, double planeTolerance,
    double oversampling, double percentile = 0.1, int maxSize = 16384);
}

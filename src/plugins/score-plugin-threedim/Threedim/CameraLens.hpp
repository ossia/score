#pragma once
#include <array>
#include <cmath>
#include <string>

namespace Threedim
{
//! How a camera pixel maps to a viewing direction (camera frame: +X right,
//! +Y down, +Z forward, as the intrinsics fx fy cx cy).
//! - Pinhole: no distortion.
//! - Distorted: Brown-Conrady radial and tangential distortion, OpenCV's
//!   k1 k2 p1 p2 k3.
//! - Fisheye: equidistant Kannala-Brandt, theta_d = theta (1 + k1 theta^2 +
//!   k2 theta^4 + k3 theta^6 + k4 theta^8), OpenCV's fisheye model; covers
//!   fields of view up to and past 180 degrees.
struct CameraLens
{
  enum Model
  {
    Pinhole,
    Distorted,
    Fisheye
  } model{Pinhole};
  std::array<double, 5> k{};

  bool operator==(const CameraLens&) const noexcept = default;

  //! Intrinsics for a horizontal field of view over `width` pixels, centred.
  static std::array<double, 4> intrinsicsFromFov(Model m, double fovDeg, int width, int height)
  {
    const double half = fovDeg * M_PI / 360.;
    const double f = m == Fisheye ? (width * 0.5) / half : (width * 0.5) / std::tan(half);
    return {f, f, (width - 1) * 0.5, (height - 1) * 0.5};
  }

  //! Unit direction of pixel (px, py).
  std::array<double, 3> unproject(const std::array<double, 4>& K, double px, double py) const
  {
    const double xd = (px - K[2]) / K[0], yd = (py - K[3]) / K[1];
    if(model == Fisheye)
    {
      const double rd = std::sqrt(xd * xd + yd * yd);
      if(rd < 1e-12)
        return {0., 0., 1.};
      double th = rd;
      for(int i = 0; i < 20; i++)
      {
        const double t2 = th * th;
        const double f = th * (1. + t2 * (k[0] + t2 * (k[1] + t2 * (k[2] + t2 * k[3])))) - rd;
        const double df
            = 1. + t2 * (3. * k[0] + t2 * (5. * k[1] + t2 * (7. * k[2] + t2 * 9. * k[3])));
        if(std::abs(df) < 1e-12)
          break;
        const double step = f / df;
        th -= step;
        if(std::abs(step) < 1e-12)
          break;
      }
      const double s = std::sin(th) / rd;
      return {xd * s, yd * s, std::cos(th)};
    }
    double x = xd, y = yd;
    if(model == Distorted)
      for(int i = 0; i < 20; i++)
      {
        const double r2 = x * x + y * y;
        const double rad = 1. + r2 * (k[0] + r2 * (k[1] + r2 * k[4]));
        const double dx = 2. * k[2] * x * y + k[3] * (r2 + 2. * x * x);
        const double dy = k[2] * (r2 + 2. * y * y) + 2. * k[3] * x * y;
        const double nx = (xd - dx) / rad, ny = (yd - dy) / rad;
        const bool done = std::abs(nx - x) < 1e-12 && std::abs(ny - y) < 1e-12;
        x = nx;
        y = ny;
        if(done)
          break;
      }
    const double n = std::sqrt(x * x + y * y + 1.);
    return {x / n, y / n, 1. / n};
  }

  //! Pixel of a direction (z > 0 for the pinhole models); false when behind.
  bool project(
      const std::array<double, 4>& K, const std::array<double, 3>& d, double& px,
      double& py) const
  {
    double xd, yd;
    if(model == Fisheye)
    {
      const double r = std::sqrt(d[0] * d[0] + d[1] * d[1]);
      const double th = std::atan2(r, d[2]);
      const double t2 = th * th;
      const double thd = th * (1. + t2 * (k[0] + t2 * (k[1] + t2 * (k[2] + t2 * k[3]))));
      const double s = r > 1e-12 ? thd / r : 1.;
      xd = d[0] * s;
      yd = d[1] * s;
    }
    else
    {
      if(!(d[2] > 1e-12))
        return false;
      const double x = d[0] / d[2], y = d[1] / d[2];
      xd = x;
      yd = y;
      if(model == Distorted)
      {
        const double r2 = x * x + y * y;
        const double rad = 1. + r2 * (k[0] + r2 * (k[1] + r2 * k[4]));
        xd = x * rad + 2. * k[2] * x * y + k[3] * (r2 + 2. * x * x);
        yd = y * rad + k[2] * (r2 + 2. * y * y) + 2. * k[3] * x * y;
      }
    }
    px = xd * K[0] + K[2];
    py = yd * K[1] + K[3];
    return true;
  }

  static const char* name(Model m)
  {
    return m == Fisheye ? "fisheye" : m == Distorted ? "distorted" : "pinhole";
  }
  static Model fromName(const std::string& s)
  {
    return s == "fisheye" ? Fisheye : s == "distorted" ? Distorted : Pinhole;
  }
};
}

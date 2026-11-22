#pragma once
#include <array>
#include <string>
#include <vector>

namespace Threedim
{
//! A viewer-camera direction map of a projector on flat surfaces, rebuilt
//! from one homography per surface: on a plane, projector pixel -> viewing
//! direction is a homography d ~ H p, p = ((x - w/2) / S, (y - h/2) / S, 1),
//! S = max(w, h). Up to maxPlanes are found by RANSAC on the measured pixels
//! (4-point DLT, inliers within the angular threshold, refit on the inliers),
//! one after the other on what the previous ones left. With two, the map is
//! split along the image of the room corner, the fixed line of H2^-1 H1, and
//! each side takes its plane's model; the side assignment is the one that
//! more measured pixels agree with. Every pixel then comes from a model: the
//! holes the camera did not see are filled and the measurement noise is gone.
//! At most two planes are fitted. ok only when the models explain at least
//! 80 % of the measured pixels: else the map is not of flat surfaces and the
//! measurement is to be kept as it is.
struct PlaneCompletion
{
  bool ok{};
  std::string status;
  int width{}, height{};
  //! RGBA per pixel, unit direction, w = 1.
  std::vector<float> directions;
  //! Per plane: the homography (row major), its inliers among the samples and
  //! their median angular error.
  std::vector<std::array<double, 9>> homographies;
  std::vector<int> inliers;
  std::vector<double> medianArcmin;
  //! Fraction of the measured pixels that a model reproduces within twice the
  //! threshold.
  double explained{};
};

PlaneCompletion completeFromPlanes(
    const std::vector<float>& directions, int width, int height,
    double thresholdArcmin = 6., int maxPlanes = 2);

//! How well up to two planes explain a direction map, without building it:
//! the mean truncated squared angular error (MSAC), in units of the
//! threshold, 0 = every pixel on a plane, 1 = none; negative without enough
//! measured pixels.
double planeResidual(
    const std::vector<float>& directions, int width, int height,
    double thresholdArcmin = 6., int maxPlanes = 2);
}

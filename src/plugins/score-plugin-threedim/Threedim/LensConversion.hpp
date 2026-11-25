#pragma once
#include <Threedim/CameraLens.hpp>

#include <QString>

#include <array>
#include <optional>
#include <vector>

namespace Threedim
{
//! The camera a direction map was measured with.
struct MeasuredCamera
{
  std::array<double, 4> intrinsics{};
  CameraLens lens;
  int width{}, height{};
};

//! name.measured.json, written next to name.measured.*.pfm, replaced
//! atomically; a file that does not parse, or of no size or focal length,
//! loads as nothing.
bool saveMeasuredCamera(const QString& base, const MeasuredCamera& c);
std::optional<MeasuredCamera> loadMeasuredCamera(const QString& base);

//! A direction map (RGBA, world frame, w = valid) made with `from`,
//! re-expressed with `to`: each direction is taken back to its camera pixel
//! and unprojected through the other lens. worldFromCamera is the rotation
//! the directions were turned by (column major 3x3 of a 4x4).
std::vector<float> convertLens(
    const std::vector<float>& directions, const MeasuredCamera& from,
    const MeasuredCamera& to, const std::array<float, 16>& worldFromCamera);

//! Estimates the lens's first radial coefficient from flat surfaces: the k1
//! (Distorted: Brown-Conrady k1; Fisheye: Kannala-Brandt k1) for which the
//! maps, re-expressed through it, are best explained by planes (planeResidual,
//! which counts the pixels no plane explains): a scan of k1, then a golden
//! section around its best. Maps are subsampled for speed.
struct LensEstimate
{
  bool ok{};
  double k1{};
  //! planeResidual at k1 and at 0, averaged over the maps.
  double residual{};
  double residualAtZero{};
};
LensEstimate estimateRadialDistortion(
    const std::vector<std::vector<float>>& maps, const std::vector<std::array<int, 2>>& sizes,
    const MeasuredCamera& measured, CameraLens::Model model,
    const std::array<double, 4>& intrinsics, const std::array<float, 16>& worldFromCamera);
}

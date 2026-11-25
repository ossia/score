#pragma once
#include <Threedim/RoomGeometry.hpp>

#include <array>
#include <string>
#include <vector>

namespace Threedim
{
//! Refines the camera's pose in the room from the viewer-camera direction
//! maps of the outputs. Cast from the right pose, the points of one output
//! all come from one projector: they are explained by a single 3x4
//! projection of the output's pixels. A wrong pose bends them along the
//! room's corners and slopes, which no projector explains. The pose
//! (position and Euler degrees) minimising the outputs' projector fit
//! residual (DLT, in output pixels) is searched with Nelder-Mead from the
//! given one. An output on a single plane is explained by every pose and is
//! left out.
struct PoseFit
{
  //! A pose that explains the outputs at least 5 % better than the given one;
  //! else position and rotation are the given ones.
  bool ok{};
  std::string status;
  std::array<float, 3> position{}, rotation{};
  //! Mean projector fit residual over the outputs, in output pixels.
  double residualBefore{}, residualAfter{};
  int outputsUsed{};
  //! Outputs whose points lie on one plane: any projector explains them.
  int outputsPlanar{};
};

struct DirectionMap
{
  const std::vector<float>* directions{};
  int width{}, height{};
};

PoseFit refineCameraPose(
    const std::vector<DirectionMap>& maps, const RoomGeometry& room,
    const std::array<float, 3>& position, const std::array<float, 3>& rotation,
    bool refinePosition = true);

//! The rotation taking the directions of one measurement of an output onto
//! another of the same output (RGBA maps of the same size, w = valid): the
//! camera turned between two runs from one place, by RANSAC over the pixels
//! valid in both (score::gfx::registration); median angle left after it, in
//! arcmin.
struct DirectionAlignment
{
  bool ok{};
  //! Row major.
  std::array<double, 9> rotation{1, 0, 0, 0, 1, 0, 0, 0, 1};
  double angleDeg{};
  double residualArcmin{};
  int pairs{}, inliers{};
};
DirectionAlignment alignDirections(
    const std::vector<std::pair<const std::vector<float>*, const std::vector<float>*>>& maps);
void rotateDirections(std::vector<float>& map, const std::array<double, 9>& R);

//! The projector fit residual of one output's points (RGBA world points per
//! output pixel, w = valid), in output pixels; negative without enough points.
double projectorFitResidual(const std::vector<float>& points, int width, int height);
}

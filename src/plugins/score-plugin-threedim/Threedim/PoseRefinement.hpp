#pragma once
#include <Threedim/RoomGeometry.hpp>

#include <array>
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
//! given one.
struct PoseFit
{
  bool ok{};
  std::array<float, 3> position{}, rotation{};
  //! Mean projector fit residual over the outputs, in output pixels.
  double residualBefore{}, residualAfter{};
  int outputsUsed{};
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

//! The projector fit residual of one output's points (RGBA world points per
//! output pixel, w = valid), in output pixels; negative without enough points.
double projectorFitResidual(const std::vector<float>& points, int width, int height);
}

#pragma once
#include <score_plugin_gfx_export.h>
#include <Gfx/Calibration/ProjectorModel.hpp>

// EPnP: Lepetit, Moreno-Noguer & Fua, "EPnP: An Accurate O(n) Solution to the
// PnP Problem", IJCV 81(2):155-166, 2009, doi:10.1007/s11263-008-0152-6.
// Four control points from the PCA of the world points; their camera-frame
// coordinates span the null space of M; the betas for 1 to 4 null vectors
// come from the control point distances, refined by Gauss-Newton; the lowest
// reprojection error wins. The beta approximations follow the authors' C++
// code as shipped in OpenCV (modules/calib3d/src/epnp.cpp).
namespace score::gfx::procam
{
namespace epnp
{
//! [R | t] from 4+ non-coplanar points on normalised image coordinates.
SCORE_PLUGIN_GFX_EXPORT bool solve(
    const std::vector<Vec3>& X, const std::vector<Vec2>& xn, const std::vector<int>& idx,
    Mat3& R, Vec3& t);
}

//! Pose only, the intrinsics of `known` being fixed, from 4+ points: EPnP,
//! or posePlanar for coplanar points, then Levenberg-Marquardt on R and t.
//! With 6+ points, the better of this and solvePose (DLT) is kept.
SCORE_PLUGIN_GFX_EXPORT Result solvePoseAny(
    const std::vector<Vec3>& X, const std::vector<Vec2>& x, const Model& known,
    const Options& opt = {});
}

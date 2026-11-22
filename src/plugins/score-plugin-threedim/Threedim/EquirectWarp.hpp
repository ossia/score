#pragma once
#include <QRectF>
#include <QString>

#include <array>
#include <vector>

namespace Threedim
{
//! UV warp of a projector onto an equirectangular canvas, from its direction
//! map (RGBA per projector pixel, top row first: the direction from the viewer,
//! w = valid), in the canvas convention of Gfx::CanvasLayout: longitude 0 at
//! -Z, +X to the right, +Y up. The UV is in the output's source rect.
//!
//! The longitude is unwrapped around the map's circular mean, so that it stays
//! continuous across the +-180 degree seam for the warp interpolation: u may
//! leave [0, 1], which an output whose canvas projection is Equirectangular
//! wraps (its warp taps are unwrapped against the centre one, then wrapped
//! before the corner pin).
//!
//! The warp holds directions only: it is right for a viewer at the camera.
//! A camera away from the viewer needs the points map (castDirections,
//! savePointsMap). `rotation` (row major) turns the directions into the room
//! frame first, as castDirections does.
//!
//! An output whose map goes round a pole (a floor or a ceiling seen from
//! inside) has no continuous longitude: enclosesPole is set and the warp is
//! not to be used; its points map is.
struct EquirectWarp
{
  int width{}, height{};
  //! u, v per pixel.
  std::vector<float> uv;
  std::vector<float> valid;
  bool enclosesPole{};
};

EquirectWarp equirectWarp(
    const std::vector<float>& directions, int width, int height,
    QRectF sourceRect = QRectF{0, 0, 1, 1}, const std::array<double, 9>* rotation = nullptr);

//! Writes base.warp.pfm and base.warp.valid.pfm, and lists both in
//! base.json's "files" so that the outputs following the calibration take them.
//! Refuses a warp that encloses a pole, and a base.json missing or invalid.
bool saveEquirectWarp(const QString& base, const EquirectWarp& w, QString* error = nullptr);

//! Writes base.points.pfm and base.points.valid.pfm from a world-point map
//! (RGBA, w = valid, top row first) and lists them in base.json: the outputs
//! that follow the calibration then look the canvas up from the viewer.
bool savePointsMap(
    const QString& base, const std::vector<float>& points, int width, int height,
    QString* error = nullptr);
}

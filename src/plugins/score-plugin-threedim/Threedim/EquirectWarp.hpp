#pragma once
#include <QRectF>
#include <QString>

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
//! wraps.
struct EquirectWarp
{
  int width{}, height{};
  //! u, v per pixel.
  std::vector<float> uv;
  std::vector<float> valid;
};

EquirectWarp equirectWarp(
    const std::vector<float>& directions, int width, int height,
    QRectF sourceRect = QRectF{0, 0, 1, 1});

//! Writes base.warp.pfm and base.warp.valid.pfm, and lists both in
//! base.json's "files" so that the outputs following the calibration take them.
bool saveEquirectWarp(const QString& base, const EquirectWarp& w, QString* error = nullptr);
}

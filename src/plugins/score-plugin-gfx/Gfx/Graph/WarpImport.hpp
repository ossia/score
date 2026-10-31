#pragma once
#include <Gfx/Graph/FloatImageIO.hpp>

#include <vector>

namespace score::gfx
{

//! Calibration of one output, in CONVENTIONS.md form: RGBA32F, rows top to bottom.
struct WarpImport
{
  //! 2D: xy = source UV (v = 0 at the top). 3D: xyz = world point. w = valid.
  FloatImage map;
  //! Blend weight in rgb, empty when the file has none.
  FloatImage alpha;
  //! Black level in rgb, empty when the file has none.
  FloatImage beta;
  bool threeD{};
  QString region;
  //! The output resolution the file describes.
  int width{};
  int height{};
};

//! Paul Bourke's warp mesh (`.data`: mesh type, "nx ny", then nx * ny lines of
//! "x y u v intensity", x in [-aspect, aspect], y in [-1, 1] up, v up, a
//! negative intensity drops the node), rasterised at width x height. The
//! intensity becomes the alpha map. A width or height <= 0 picks 1024 wide at
//! the mesh's aspect.
SCORE_PLUGIN_GFX_EXPORT
WarpImport importBourkeMesh(
    const QByteArray& text, int width, int height, QString* error = nullptr);
SCORE_PLUGIN_GFX_EXPORT
WarpImport
loadBourkeMesh(const QString& path, int width, int height, QString* error = nullptr);

//! Every region of an MPCDI archive (mpcdi.xml + geometryWarpFile PFM, alphaMap,
//! betaMap; 2D Media and Advanced 3D profiles).
SCORE_PLUGIN_GFX_EXPORT
std::vector<WarpImport> loadMPCDI(const QString& path, QString* error = nullptr);

//! `file.mpcdi`, `file.mpcdi#region` or a Bourke `file.data`, as the map of the
//! given kind; for anything else, loadFloatImageRGBA.
enum class WarpMapKind
{
  Map,
  Alpha,
  Beta
};
SCORE_PLUGIN_GFX_EXPORT
FloatImage loadWarpMap(const QString& path, WarpMapKind kind, QString* error = nullptr);
}

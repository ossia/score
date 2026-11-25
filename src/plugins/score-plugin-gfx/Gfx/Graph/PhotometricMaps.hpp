#pragma once
#include <Gfx/Calibration/Photometric.hpp>
#include <Gfx/Graph/FloatImageIO.hpp>

#include <array>
#include <vector>

namespace score::gfx
{

//! The encode of a calibrated output: normalised linear light t in [0, 1] ->
//! projector code, per channel, `size` samples at t = i / (size - 1).
struct ResponseTable
{
  int size{};
  //! size * 3, interleaved rgb.
  std::vector<float> inverse;
  //! The projector's colour mixing, inverted: shown light -> per-channel light.
  photometric::MixingInverse mixing;

  bool valid() const noexcept
  {
    return size >= 2 && inverse.size() == std::size_t(size) * 3;
  }
};

//! `name.response.json` (the "inverse" table) or a 1D `.cube` (LUT_1D_SIZE,
//! read as the encode curve itself).
SCORE_PLUGIN_GFX_EXPORT
ResponseTable loadResponseTable(const QString& path, QString* error = nullptr);

struct ProjectorResponseFile
{
  //! red, green, blue, grey (grey optional: empty x).
  std::array<photometric::ChannelResponse, 4> channels;
  //! Max over the levels of |grey - (red + green + blue)| / |white - black|.
  double additivityError{-1.};
  photometric::ColourMixing mixing;
  //! Mean relative error on the off-grid mixed colours, additive and with `mixing`.
  double mixErrorAdditive{-1.}, mixErrorModel{-1.};
  //! Every uniform frame: projector code, mean linear camera colour minus black.
  std::vector<std::pair<std::array<float, 3>, std::array<double, 3>>> samples;
  QString linearisation;
  int inverseSize{1024};
};

//! Writes `base.response.json` and `base.response.cube` (1D, the inverse).
SCORE_PLUGIN_GFX_EXPORT
bool writeProjectorResponse(const QString& base, const ProjectorResponseFile& r);

//! Per output pixel P = M c + o, linear projector RGB from linear content.
//! rows[k] is RGBA: (M_k0, M_k1, M_k2, o_k), output pixels, rows top to bottom.
struct CompensationMaps
{
  int width{}, height{};
  std::array<FloatImage, 3> rows;

  bool valid() const noexcept
  {
    for(auto& r : rows)
      if(!r.valid() || r.width != width || r.height != height || r.channels != 4)
        return false;
    return width > 0 && height > 0;
  }
};

struct CompensationInfo
{
  std::array<double, 3> target{1., 1., 1.};
  double scale{1.};
  double strength{1.};
  double percentile{0.95};
  double inGamut{};
  bool black{};
  bool mixing{};
};

//! `base.comp.json` + `base.comp0.pfm`, `base.comp1.pfm`, `base.comp2.pfm`
//! (RGB rows of M) + `base.compoffset.pfm` (RGB o).
SCORE_PLUGIN_GFX_EXPORT
bool writeCompensation(
    const QString& base, const CompensationMaps& maps, const CompensationInfo& info);

//! From `base.comp.json` (or any of its PFMs).
SCORE_PLUGIN_GFX_EXPORT
CompensationMaps loadCompensation(const QString& path, QString* error = nullptr);
}

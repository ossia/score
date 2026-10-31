#pragma once
#include <QString>

#include <score_plugin_gfx_export.h>

#include <array>
#include <cstdint>
#include <vector>

namespace score::gfx
{
//! A 3D colour lookup table: size^3 RGB entries, red index fastest, then
//! green, then blue (Adobe Cube LUT Specification 1.0, 2013).
struct CubeLut
{
  QString title;
  int size{};
  std::array<float, 3> domainMin{0.f, 0.f, 0.f};
  std::array<float, 3> domainMax{1.f, 1.f, 1.f};
  std::vector<float> rgb;

  bool valid() const noexcept
  {
    return size >= 2 && rgb.size() == std::size_t(size) * size * size * 3;
  }
};

//! Reads a .cube file. A 1D table (LUT_1D_SIZE) is baked into a 3D one of
//! min(size, 65) entries per axis. LUT_1D_INPUT_RANGE / LUT_3D_INPUT_RANGE
//! (the older IRIDAS / Resolve spelling) are read as DOMAIN_MIN / DOMAIN_MAX.
SCORE_PLUGIN_GFX_EXPORT
CubeLut loadCubeLut(const QString& path, QString* error = nullptr);
SCORE_PLUGIN_GFX_EXPORT
CubeLut parseCubeLut(const QByteArray& text, QString* error = nullptr);

//! A size x size threshold matrix by the void-and-cluster method (Ulichney,
//! "The void-and-cluster method for dither array generation", Proc. SPIE 1913,
//! 1993): the rank of each pixel, as (rank + 0.5) / size^2. Deterministic, and
//! computed once per size.
SCORE_PLUGIN_GFX_EXPORT
const std::vector<float>& blueNoise(int size);
}

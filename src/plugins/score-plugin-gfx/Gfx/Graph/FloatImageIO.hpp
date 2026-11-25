#pragma once
#include <QByteArray>
#include <QString>

#include <score_plugin_gfx_export.h>

#include <vector>

class QImage;

namespace score::gfx
{
inline void setFileError(QString* error, const QString& e)
{
  if(error)
    *error = e;
}

//! Interleaved float pixels, rows from top to bottom.
struct FloatImage
{
  int width{};
  int height{};
  int channels{};
  std::vector<float> data;

  bool valid() const noexcept
  {
    return width > 0 && height > 0 && channels > 0
           && data.size() == std::size_t(width) * height * channels;
  }
  float* row(int y) noexcept { return data.data() + std::size_t(y) * width * channels; }
  const float* row(int y) const noexcept
  {
    return data.data() + std::size_t(y) * width * channels;
  }
};

//! Portable Float Map: `PF` (RGB) or `Pf` (grey), rows stored bottom to top.
SCORE_PLUGIN_GFX_EXPORT
FloatImage parsePFM(const QByteArray& bytes, QString* error = nullptr);
SCORE_PLUGIN_GFX_EXPORT
FloatImage readPFM(const QString& path, QString* error = nullptr);

//! Little-endian PFM; channels must be 1 or 3.
SCORE_PLUGIN_GFX_EXPORT
QByteArray encodePFM(const FloatImage& img);
//! Replaces `path` atomically: readers see the old file or the new one.
SCORE_PLUGIN_GFX_EXPORT
bool writePFM(const QString& path, const FloatImage& img, QString* error = nullptr);

//! `name.xyz.pfm` / `name.pfm` -> `name.valid.pfm`.
SCORE_PLUGIN_GFX_EXPORT
QString validMapPath(const QString& pfmPath);

//! RGBA float image from a PFM (a 3-channel one takes its fourth channel from
//! the `.valid.pfm` next to it when there is one, 1 otherwise; a grey one is
//! replicated) or from any image QImage reads (8 / 16-bit PNG, TIFF, EXR when
//! the Qt image plug-in exists), normalised to [0, 1].
SCORE_PLUGIN_GFX_EXPORT
FloatImage loadFloatImageRGBA(const QString& path, QString* error = nullptr);

//! Writes the RGB channels to `base.xyz.pfm` and the alpha channel to
//! `base.valid.pfm`; a grey or RGB image is written as `base.pfm`.
SCORE_PLUGIN_GFX_EXPORT
bool writeFloatImageSplit(
    const QString& basePath, const FloatImage& img, QString* error = nullptr);

SCORE_PLUGIN_GFX_EXPORT
FloatImage toRGBA(const FloatImage& img);

//! Any QImage as RGBA32F, normalised to [0, 1].
SCORE_PLUGIN_GFX_EXPORT
FloatImage toFloatImageRGBA(const QImage& img);
}

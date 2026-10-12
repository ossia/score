#pragma once
#include <QtGui/private/qrhi_p.h>

#include <QByteArray>
#include <QImage>

#include <cstring>

namespace score::gfx
{
/**
 * Row order of the 2D image textures that travel between nodes.
 *
 * A render target holds its image bottom row first in memory on OpenGL, Metal
 * and Direct3D -- the GL framebuffer origin, and the isf_vertShaderFinish()
 * negation on the others: QRhi::isYUpInNDC() -- and top row first on Vulkan.
 * Every 2D image texture a node publishes has the row order of a rendered one,
 * whoever produced it, so a consumer (IMG_NORM_PIXEL, the output blits, a
 * STATIC binding, a readback, a material slot) never needs to know whether it
 * was drawn or uploaded.
 *
 * An image coming from the CPU is top row first (QImage, decoded files, halp
 * CPU textures), so a producer publishing it reverses its rows on upload when
 * uploadReversesRows() is true. Data tables (LUTs addressed by a parameter,
 * not by an image position) are not images and keep their memory order: their
 * row 0 is at v = 0 on every backend, for a raw texture() read.
 */
inline bool uploadReversesRows(const QRhi& rhi) noexcept
{
  return rhi.isYUpInNDC();
}

//! dst = src with its rows in reverse order: one memcpy per row.
inline void
copyRowsReversed(void* dst, const void* src, qsizetype rowBytes, int rows) noexcept
{
  auto* d = static_cast<char*>(dst);
  auto* s = static_cast<const char*>(src) + qsizetype(rows - 1) * rowBytes;
  for(int y = 0; y < rows; ++y, d += rowBytes, s -= rowBytes)
    std::memcpy(d, s, rowBytes);
}

//! A tightly packed, top-row-first image of @p rows rows in the published row
//! order. Returns @p bytes untouched where uploads keep their rows.
inline QByteArray
bytesInPublishedRowOrder(const QRhi& rhi, QByteArray bytes, int rows)
{
  if(!uploadReversesRows(rhi) || rows <= 1 || bytes.size() % rows != 0)
    return bytes;
  QByteArray out(bytes.size(), Qt::Uninitialized);
  copyRowsReversed(out.data(), bytes.constData(), bytes.size() / rows, rows);
  return out;
}

//! @p img in the published row order: flipped where uploads reverse rows.
inline QImage imageInPublishedRowOrder(const QRhi& rhi, QImage img)
{
  if(!uploadReversesRows(rhi) || img.height() <= 1)
    return img;
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
  return std::move(img).flipped(Qt::Vertical);
#else
  return std::move(img).mirrored(false, true);
#endif
}
}

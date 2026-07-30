#pragma once

/**
 * @file VideoPixelFormatQRhi.hpp
 * @brief Bridge between a CPU/DMA buffer layout and a GPU texture format.
 *
 * These are two different axes, and conflating them is a category error:
 *
 *   VideoPixelFormat   what a buffer physically holds (a card's DMA target, a
 *                      dma-buf, a shared-memory frame)
 *   QRhiTexture::Format what a GPU texture holds
 *
 * They are not in bijection. A planar layout needs several textures, one per
 * plane, so it has no single texture format -- `planeTextureFormat()` answers
 * per plane. A packed YUV layout like UYVY422 is uploaded as a wider RGBA8
 * texture and unpacked in a shader, so its texture format says nothing about its
 * colour model. And the wire-only formats (v210, r210, 12-bit packed) have no
 * texture format at all: they must be decoded first.
 *
 * This is the axis shared texture transports work in -- Spout hands over a D3D11
 * texture, Syphon an IOSurface, dma-buf import an EGLImage -- so they belong
 * here rather than being forced through the buffer vocabulary.
 *
 * Kept separate from VideoPixelFormat.hpp so the vocabulary itself stays free of
 * Qt and RHI dependencies, exactly as the libav bridge is kept separate.
 */

#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <score_plugin_gfx_export.h>

#include <QtGui/private/qrhi_p.h>

namespace score::gfx::interop
{

/// Texture format for plane `plane` of `f`, or UnknownFormat when the layout has
/// no direct GPU representation (wire-only formats) or the plane does not exist.
///
/// For packed layouts only plane 0 exists, and the answer is the format a shader
/// samples to recover the pixels -- which for packed YUV is a wider RGBA8
/// texture, not a YUV format, because QRhi has none.
SCORE_PLUGIN_GFX_EXPORT
QRhiTexture::Format
planeTextureFormat(VideoPixelFormat f, int plane) noexcept;

/// Width in texels of plane `plane` for a frame `width` pixels wide. Packed YUV
/// needs fewer texels than pixels because several components share one texel;
/// chroma planes are narrowed by the subsampling.
SCORE_PLUGIN_GFX_EXPORT
uint32_t planeTextureWidth(VideoPixelFormat f, int plane, uint32_t width) noexcept;

/// Height in texels of plane `plane` for a frame `height` pixels tall.
SCORE_PLUGIN_GFX_EXPORT
uint32_t planeTextureHeight(VideoPixelFormat f, int plane, uint32_t height) noexcept;

/// The layout a texture of this format holds, for the transports that hand over
/// a texture rather than a buffer (Spout, Syphon, dma-buf import). Only the
/// unambiguous RGB formats round-trip: a shader-unpacked YUV texture is RGBA8
/// like any other, so the direction cannot be inverted for those.
SCORE_PLUGIN_GFX_EXPORT
VideoPixelFormat fromTextureFormat(QRhiTexture::Format f) noexcept;

} // namespace score::gfx::interop

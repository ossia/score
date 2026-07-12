#pragma once

#include <Gfx/Graph/interop/DrmPixelFormat.hpp>
#include <Gfx/Graph/interop/VideoPixelFormatAV.hpp>

// SPDX-License-Identifier: GPL-3.0-or-later
//
// DRM fourcc <-> AVPixelFormat <-> SPA video format mapping: the one
// authoritative table, shared by Gfx/Pipewire, Gfx/WindowCapture and
// tests/PipewireRoundtrip.cpp.
//
// Conventions (little-endian, per <drm/drm_fourcc.h> — hardware-verified by
// the PipewireRoundtrip DMA-BUF cells):
//   - DRM fourccs name components from MSB to LSB of the packed word, so
//     memory order is REVERSED: DRM_FORMAT_ABGR8888 ('AB24') is
//     [R,G,B,A] bytes in memory == AV_PIX_FMT_RGBA == SPA_VIDEO_FORMAT_RGBA.
//   - 10-bit: DRM_FORMAT_ARGB2101010 ('AR30') has R in bits 29-20 ==
//     AV_PIX_FMT_X2RGB10LE == SPA_VIDEO_FORMAT_xRGB_210LE. 'AB30' is the
//     mirrored format, not RGB10A2.
//
// The DRM tokens are hardcoded so score builds without <drm/drm_fourcc.h>.
// The SPA mapping is only compiled when the pipewire/SPA headers are
// available (every current consumer is pipewire-gated anyway).

extern "C" {
#include <libavutil/pixfmt.h>
}

#include <cstdint>

#if __has_include(<spa/param/video/raw.h>)
#include <spa/param/video/raw.h>
#define SCORE_GFX_HAS_SPA_RAW 1
#endif

namespace score::gfx::interop
{

constexpr uint32_t drmFourcc(char a, char b, char c, char d) noexcept
{
  return uint32_t(uint8_t(a)) | (uint32_t(uint8_t(b)) << 8)
         | (uint32_t(uint8_t(c)) << 16) | (uint32_t(uint8_t(d)) << 24);
}

// clang-format off
inline constexpr uint32_t DRM_ABGR8888      = drmFourcc('A','B','2','4'); // [R,G,B,A] memory
inline constexpr uint32_t DRM_ARGB8888      = drmFourcc('A','R','2','4'); // [B,G,R,A] memory
inline constexpr uint32_t DRM_XBGR8888      = drmFourcc('X','B','2','4');
inline constexpr uint32_t DRM_XRGB8888      = drmFourcc('X','R','2','4');
inline constexpr uint32_t DRM_ARGB2101010   = drmFourcc('A','R','3','0'); // R bits 29-20
inline constexpr uint32_t DRM_ABGR2101010   = drmFourcc('A','B','3','0'); // B bits 29-20
inline constexpr uint32_t DRM_ABGR16161616F = drmFourcc('A','B','4','H');
inline constexpr uint32_t DRM_BGR888        = drmFourcc('B','G','2','4'); // [R,G,B] memory
inline constexpr uint32_t DRM_NV12          = drmFourcc('N','V','1','2');
inline constexpr uint32_t DRM_P010          = drmFourcc('P','0','1','0');
inline constexpr uint32_t DRM_P210          = drmFourcc('P','2','1','0');
inline constexpr uint32_t DRM_YUV420        = drmFourcc('Y','U','1','2');
inline constexpr uint32_t DRM_YVU420        = drmFourcc('Y','V','1','2');
inline constexpr uint32_t DRM_YUYV          = drmFourcc('Y','U','Y','V');
inline constexpr uint32_t DRM_UYVY          = drmFourcc('U','Y','V','Y');
// clang-format on

/** DRM fourcc -> AVPixelFormat. Delegates to the vocabulary: the fourcc names a
 *  layout, and the layout knows its AVPixelFormat, so there is no second opinion
 *  about either here. Verified to answer exactly as the previous table did for
 *  every fourcc it handled, including AV_PIX_FMT_NONE for DRM_YVU420, whose
 *  plane-swapped layout FFmpeg cannot name -- callers must swap U and V, and
 *  interop::fromDrmFourcc() now tells them so explicitly by answering YVU420P. */
inline AVPixelFormat drmFourccToAv(uint32_t fourcc) noexcept
{
  return score::gfx::interop::toAVPixelFormat(
      score::gfx::interop::fromDrmFourcc(fourcc));
}

/** AVPixelFormat -> DRM fourcc. 0 if unmapped. */
inline uint32_t avToDrmFourcc(AVPixelFormat fmt) noexcept
{
  return score::gfx::interop::toDrmFourcc(
      score::gfx::interop::fromAVPixelFormat(fmt));
}

/** DRM fourcc -> the buffer layout, for callers that want the layout rather than
 *  an AVPixelFormat -- notably the ones that can import a dma-buf directly and
 *  need the plane geometry rather than a decode target. */
inline score::gfx::interop::VideoPixelFormat
drmFourccToVideoPixelFormat(uint32_t fourcc) noexcept
{
  return score::gfx::interop::fromDrmFourcc(fourcc);
}

#if defined(SCORE_GFX_HAS_SPA_RAW)
/** SPA video format -> DRM fourcc. 0 if unmapped. Superset of the former
 *  PipewireFormats::toDrmFourcc and WindowCapture_pipewire tables. */
/** SPA video format -> the buffer layout. SPA formats are defined in DRM terms,
 *  so this goes through the fourcc rather than maintaining a third table. */
inline score::gfx::interop::VideoPixelFormat
spaToVideoPixelFormat(uint32_t spaFmt) noexcept;

inline uint32_t spaToDrmFourcc(uint32_t spaFmt) noexcept
{
  switch(spaFmt)
  {
    case SPA_VIDEO_FORMAT_RGBA:        return DRM_ABGR8888;
    case SPA_VIDEO_FORMAT_BGRA:        return DRM_ARGB8888;
    case SPA_VIDEO_FORMAT_RGBx:        return DRM_XBGR8888;
    case SPA_VIDEO_FORMAT_BGRx:        return DRM_XRGB8888;
    case SPA_VIDEO_FORMAT_xRGB_210LE:  return DRM_ARGB2101010;
    case SPA_VIDEO_FORMAT_xBGR_210LE:  return DRM_ABGR2101010;
    case SPA_VIDEO_FORMAT_RGBA_F16:    return DRM_ABGR16161616F;
    case SPA_VIDEO_FORMAT_RGB:         return DRM_BGR888;
    case SPA_VIDEO_FORMAT_NV12:        return DRM_NV12;
    case SPA_VIDEO_FORMAT_P010_10LE:   return DRM_P010;
    case SPA_VIDEO_FORMAT_I420:        return DRM_YUV420;
    case SPA_VIDEO_FORMAT_YV12:        return DRM_YVU420;
    case SPA_VIDEO_FORMAT_YUY2:        return DRM_YUYV;
    case SPA_VIDEO_FORMAT_UYVY:        return DRM_UYVY;
    default:                           return 0;
  }
}

inline score::gfx::interop::VideoPixelFormat
spaToVideoPixelFormat(uint32_t spaFmt) noexcept
{
  return score::gfx::interop::fromDrmFourcc(spaToDrmFourcc(spaFmt));
}
#endif

} // namespace score::gfx::interop

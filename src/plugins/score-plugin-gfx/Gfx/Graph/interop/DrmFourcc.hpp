#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
//
// DRM fourcc <-> AVPixelFormat <-> SPA video format mapping — the ONE
// authoritative table. Previously duplicated (and disagreeing) between
// Gfx/Pipewire/PipewireFormats.hpp, Gfx/WindowCapture/WindowCapture_pipewire.cpp
// and tests/PipewireRoundtrip.cpp.
//
// Conventions (little-endian, per <drm/drm_fourcc.h> — hardware-verified by
// the PipewireRoundtrip DMA-BUF cells):
//   - DRM fourccs name components from MSB to LSB of the packed word, so
//     memory order is REVERSED: DRM_FORMAT_ABGR8888 ('AB24') is
//     [R,G,B,A] bytes in memory == AV_PIX_FMT_RGBA == SPA_VIDEO_FORMAT_RGBA.
//   - 10-bit: DRM_FORMAT_ARGB2101010 ('AR30') has R in bits 29-20 ==
//     AV_PIX_FMT_X2RGB10LE == SPA_VIDEO_FORMAT_xRGB_210LE. (The 'AB30'
//     spelling previously used for RGB10A2 in PipewireFormats.hpp was the
//     mirrored format — latent DMA-BUF bug, fixed by unification.)
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

/** DRM fourcc -> AVPixelFormat (memory-layout-equivalent). X-variants map to
 *  the alpha-ignored AV formats (RGB0/BGR0). AV_PIX_FMT_NONE if unmapped
 *  (e.g. half-float: no portable packed-half AVFrame format exists). */
inline AVPixelFormat drmFourccToAv(uint32_t fourcc) noexcept
{
  switch(fourcc)
  {
    case DRM_ABGR8888:    return AV_PIX_FMT_RGBA;
    case DRM_ARGB8888:    return AV_PIX_FMT_BGRA;
    case DRM_XBGR8888:    return AV_PIX_FMT_RGB0;
    case DRM_XRGB8888:    return AV_PIX_FMT_BGR0;
    case DRM_ARGB2101010: return AV_PIX_FMT_X2RGB10LE;
    case DRM_ABGR2101010: return AV_PIX_FMT_X2BGR10LE;
    case DRM_BGR888:      return AV_PIX_FMT_RGB24;
    case DRM_NV12:        return AV_PIX_FMT_NV12;
    case DRM_P010:        return AV_PIX_FMT_P010LE;
    case DRM_P210:        return AV_PIX_FMT_P210LE;
    case DRM_YUV420:      return AV_PIX_FMT_YUV420P;
    case DRM_YUYV:        return AV_PIX_FMT_YUYV422;
    case DRM_UYVY:        return AV_PIX_FMT_UYVY422;
    // DRM_YVU420: plane-swapped I420; callers must swap U/V explicitly.
    default:              return AV_PIX_FMT_NONE;
  }
}

/** AVPixelFormat -> DRM fourcc. 0 if unmapped. */
inline uint32_t avToDrmFourcc(AVPixelFormat fmt) noexcept
{
  switch(fmt)
  {
    case AV_PIX_FMT_RGBA:       return DRM_ABGR8888;
    case AV_PIX_FMT_BGRA:       return DRM_ARGB8888;
    case AV_PIX_FMT_RGB0:       return DRM_XBGR8888;
    case AV_PIX_FMT_BGR0:       return DRM_XRGB8888;
    case AV_PIX_FMT_X2RGB10LE:  return DRM_ARGB2101010;
    case AV_PIX_FMT_X2BGR10LE:  return DRM_ABGR2101010;
    case AV_PIX_FMT_RGB24:      return DRM_BGR888;
    case AV_PIX_FMT_NV12:       return DRM_NV12;
    case AV_PIX_FMT_P010LE:     return DRM_P010;
    case AV_PIX_FMT_P210LE:     return DRM_P210;
    case AV_PIX_FMT_YUV420P:    return DRM_YUV420;
    case AV_PIX_FMT_YUYV422:    return DRM_YUYV;
    case AV_PIX_FMT_UYVY422:    return DRM_UYVY;
    default:                    return 0;
  }
}

#if defined(SCORE_GFX_HAS_SPA_RAW)
/** SPA video format -> DRM fourcc. 0 if unmapped. Superset of the former
 *  PipewireFormats::toDrmFourcc and WindowCapture_pipewire tables. */
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
#endif

} // namespace score::gfx::interop

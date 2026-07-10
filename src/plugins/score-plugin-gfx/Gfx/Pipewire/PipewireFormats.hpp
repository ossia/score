#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
//
// Shared format-conversion utilities for the PipeWire video I/O
// devices. Maps between:
//
//   - SPA_VIDEO_FORMAT_* (pipewire wire format)
//   - AVPixelFormat (FFmpeg, used on the input AVFrame side)
//   - QRhiTexture::Format (used on the output rendering side)
//
// Header-only — the body is small enough that having two TUs include
// it is cheaper than a separate .cpp. Both InputDevice and OutputDevice
// pull it in.

#include <Gfx/Graph/interop/DrmFourcc.hpp>

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>

extern "C" {
#include <libavutil/pixfmt.h>
}

#include <private/qrhi_p.h>

#include <QString>

#include <cstdint>

namespace Gfx::PipeWire::formats
{

/** High-level format tag exposed in the UI / URL string. Each value
 *  has a deterministic mapping to one SPA format, one AVPixelFormat,
 *  and (for output paths) one QRhiTexture::Format.
 *
 *  The set is intentionally small: just the formats score's render
 *  pipeline can realistically produce or consume without bespoke
 *  shader work. Add more lazily as consumers materialise. */
enum class Tag : uint8_t
{
  RGBA8 = 0,    /**< 8-bit packed RGBA, default. */
  BGRA8,        /**< 8-bit packed BGRA — match for D3D11 / many cameras. */
  RGB10A2,      /**< 10-bit RGB + 2-bit alpha, packed (HDR ready). */
  BGR10A2,      /**< 10-bit BGR + 2-bit alpha. */
  RGBA16F,      /**< Half-float per channel — HDR scene-referred / wide gamut. */
  RGBA32F,      /**< Full float per channel — uncompressed HDR. */
  P010,         /**< NV12-shape with 10-bit upshifted to 16-bit lanes. */
  P210,         /**< P010-shape with 4:2:2 chroma (full chroma height). */
  YUV420P,      /**< Three-plane 4:2:0, 8-bit (I420 plane order: Y,U,V). */
  YV12,         /**< Three-plane 4:2:0, 8-bit, V before U on the wire. */
  NV12,         /**< Two-plane 4:2:0, 8-bit. */
  YUYV422,      /**< Packed 4:2:2, 8-bit. */
  UYVY422,
  RGB24,        /**< Packed 8-bit RGB without alpha. */
  Unknown
};

/** Parse a user-facing format identifier (URL query, settings combo).
 *  Case-insensitive. Returns Tag::Unknown on no match. */
inline Tag tagFromString(const QString& s)
{
  const QString k = s.toLower();
  if(k == "rgba8" || k == "rgba")
    return Tag::RGBA8;
  if(k == "bgra8" || k == "bgra")
    return Tag::BGRA8;
  if(k == "rgb10a2")
    return Tag::RGB10A2;
  if(k == "bgr10a2")
    return Tag::BGR10A2;
  if(k == "rgba16f" || k == "rgba_f16")
    return Tag::RGBA16F;
  if(k == "rgba32f" || k == "rgba_f32")
    return Tag::RGBA32F;
  if(k == "p010")
    return Tag::P010;
  if(k == "p210")
    return Tag::P210;
  if(k == "yuv420p" || k == "i420")
    return Tag::YUV420P;
  if(k == "yv12")
    return Tag::YV12;
  if(k == "nv12")
    return Tag::NV12;
  if(k == "yuyv422" || k == "yuy2")
    return Tag::YUYV422;
  if(k == "uyvy422" || k == "uyvy")
    return Tag::UYVY422;
  if(k == "rgb24" || k == "rgb")
    return Tag::RGB24;
  return Tag::Unknown;
}

/** Map a Tag to its SPA wire format. Used when building EnumFormat. */
inline spa_video_format toSpa(Tag t)
{
  switch(t)
  {
    case Tag::RGBA8:    return SPA_VIDEO_FORMAT_RGBA;
    case Tag::BGRA8:    return SPA_VIDEO_FORMAT_BGRA;
    case Tag::RGB10A2:  return SPA_VIDEO_FORMAT_xRGB_210LE;
    case Tag::BGR10A2:  return SPA_VIDEO_FORMAT_xBGR_210LE;
    case Tag::RGBA16F:  return SPA_VIDEO_FORMAT_RGBA_F16;
    case Tag::RGBA32F:  return SPA_VIDEO_FORMAT_RGBA_F32;
    case Tag::P010:     return SPA_VIDEO_FORMAT_P010_10LE;
    case Tag::P210:     return SPA_VIDEO_FORMAT_P010_10LE; // pipewire doesn't have a P210 enum
    case Tag::YUV420P:  return SPA_VIDEO_FORMAT_I420;
    case Tag::YV12:     return SPA_VIDEO_FORMAT_YV12;
    case Tag::NV12:     return SPA_VIDEO_FORMAT_NV12;
    case Tag::YUYV422:  return SPA_VIDEO_FORMAT_YUY2;
    case Tag::UYVY422:  return SPA_VIDEO_FORMAT_UYVY;
    case Tag::RGB24:    return SPA_VIDEO_FORMAT_RGB;
    case Tag::Unknown:  break;
  }
  return SPA_VIDEO_FORMAT_RGBA;
}

/** Inverse of toSpa() — used on the input side when parsing a
 *  negotiated format from `spa_format_video_raw_parse`. */
inline Tag tagFromSpa(uint32_t fmt)
{
  switch(fmt)
  {
    case SPA_VIDEO_FORMAT_RGBA:        return Tag::RGBA8;
    case SPA_VIDEO_FORMAT_BGRA:        return Tag::BGRA8;
    case SPA_VIDEO_FORMAT_xRGB_210LE:  return Tag::RGB10A2;
    case SPA_VIDEO_FORMAT_xBGR_210LE:  return Tag::BGR10A2;
    case SPA_VIDEO_FORMAT_RGBA_F16:    return Tag::RGBA16F;
    case SPA_VIDEO_FORMAT_RGBA_F32:    return Tag::RGBA32F;
    case SPA_VIDEO_FORMAT_P010_10LE:   return Tag::P010;
    case SPA_VIDEO_FORMAT_YV12:        return Tag::YV12;
    case SPA_VIDEO_FORMAT_I420:        return Tag::YUV420P;
    case SPA_VIDEO_FORMAT_NV12:        return Tag::NV12;
    case SPA_VIDEO_FORMAT_YUY2:        return Tag::YUYV422;
    case SPA_VIDEO_FORMAT_UYVY:        return Tag::UYVY422;
    case SPA_VIDEO_FORMAT_RGB:         return Tag::RGB24;
    default:                           return Tag::Unknown;
  }
}

/** Map a Tag to the matching AVPixelFormat for the FFmpeg AVFrame
 *  path consumed by score's input pipeline. Returns AV_PIX_FMT_NONE
 *  on unsupported tags. */
inline AVPixelFormat toAvPixFmt(Tag t)
{
  switch(t)
  {
    case Tag::RGBA8:    return AV_PIX_FMT_RGBA;
    case Tag::BGRA8:    return AV_PIX_FMT_BGRA;
    case Tag::RGB10A2:  return AV_PIX_FMT_X2RGB10LE;
    case Tag::BGR10A2:  return AV_PIX_FMT_X2BGR10LE;
    case Tag::RGBA16F:
      // FFmpeg 6.0+ has AV_PIX_FMT_RGBAF16; older libs fall back to
      // AV_PIX_FMT_RGBA64LE which is 16-bit integer per channel.
      // RGBA64LE is the more universally supported choice.
      return AV_PIX_FMT_RGBA64LE;
    case Tag::RGBA32F:  return AV_PIX_FMT_NONE; // not a standard AVFrame format
    case Tag::P010:     return AV_PIX_FMT_P010LE;
    case Tag::P210:     return AV_PIX_FMT_P210LE;
    case Tag::YUV420P:  return AV_PIX_FMT_YUV420P;
    // YV12 is published as YUV420P: the copy path swaps the U/V plane
    // order so the AVFrame is always I420-ordered.
    case Tag::YV12:     return AV_PIX_FMT_YUV420P;
    case Tag::NV12:     return AV_PIX_FMT_NV12;
    case Tag::YUYV422:  return AV_PIX_FMT_YUYV422;
    case Tag::UYVY422:  return AV_PIX_FMT_UYVY422;
    case Tag::RGB24:    return AV_PIX_FMT_RGB24;
    case Tag::Unknown:  break;
  }
  return AV_PIX_FMT_NONE;
}

inline Tag tagFromAvPixFmt(AVPixelFormat fmt)
{
  switch(fmt)
  {
    case AV_PIX_FMT_RGBA:        return Tag::RGBA8;
    case AV_PIX_FMT_BGRA:        return Tag::BGRA8;
    case AV_PIX_FMT_X2RGB10LE:   return Tag::RGB10A2;
    case AV_PIX_FMT_X2BGR10LE:   return Tag::BGR10A2;
    case AV_PIX_FMT_RGBA64LE:    return Tag::RGBA16F;
    case AV_PIX_FMT_P010LE:      return Tag::P010;
    case AV_PIX_FMT_P210LE:      return Tag::P210;
    case AV_PIX_FMT_YUV420P:     return Tag::YUV420P;
    case AV_PIX_FMT_NV12:        return Tag::NV12;
    case AV_PIX_FMT_YUYV422:     return Tag::YUYV422;
    case AV_PIX_FMT_UYVY422:     return Tag::UYVY422;
    case AV_PIX_FMT_RGB24:       return Tag::RGB24;
    default:                     return Tag::Unknown;
  }
}

/** Map a Tag to the matching QRhiTexture::Format for the output
 *  rendering side. Returns RGBA8 as a safe fallback. */
inline QRhiTexture::Format toQRhi(Tag t)
{
  switch(t)
  {
    case Tag::RGBA8:    return QRhiTexture::RGBA8;
    case Tag::BGRA8:    return QRhiTexture::BGRA8;
    case Tag::RGB10A2:
    case Tag::BGR10A2:  return QRhiTexture::RGB10A2;
    case Tag::RGBA16F:  return QRhiTexture::RGBA16F;
    case Tag::RGBA32F:  return QRhiTexture::RGBA32F;
    // Planar / packed YUV formats: QRhi has no native planar storage.
    // Output side cannot produce these directly; the renderer would
    // need a CPU pack pass between readback and chunk publish.
    default:            return QRhiTexture::RGBA8;
  }
}

/** Bytes-per-pixel for the packed (single-plane) variants — used to
 *  size readback chunks and stride. For planar formats returns the
 *  size of the dominant (Y) plane sample. */
inline uint32_t bytesPerPixel(Tag t)
{
  switch(t)
  {
    case Tag::RGBA8:
    case Tag::BGRA8:
    case Tag::RGB10A2:
    case Tag::BGR10A2:    return 4;
    case Tag::YUYV422:    // 4:2:2 packed: 2 bytes per pixel (Y always,
    case Tag::UYVY422:    return 2; // U/V shared across the pair)
    case Tag::RGBA16F:    return 8;
    case Tag::RGBA32F:    return 16;
    case Tag::P010:
    case Tag::P210:       return 2; // 16-bit lane per Y sample
    case Tag::YUV420P:
    case Tag::YV12:
    case Tag::NV12:       return 1; // Y plane byte
    case Tag::RGB24:      return 3;
    case Tag::Unknown:    break;
  }
  return 4;
}

/** Total bytes of one tightly-packed frame including chroma planes.
 *  bytesPerPixel() alone under-counts planar formats (it returns the
 *  Y-plane lane size), which matters both for advertising
 *  SPA_PARAM_BUFFERS_size and for validating incoming chunk sizes. */
inline uint64_t frameBytes(Tag t, int w, int h) noexcept
{
  const uint64_t wh = uint64_t(w) * uint64_t(h);
  switch(t)
  {
    case Tag::NV12:
    case Tag::YV12:
    case Tag::YUV420P: return wh * 3 / 2; // 8-bit 4:2:0
    case Tag::P010:    return wh * 3;     // 16-bit lanes, 4:2:0
    case Tag::P210:    return wh * 4;     // 16-bit lanes, 4:2:2
    default:           return wh * bytesPerPixel(t);
  }
}

/** True if the SPA format is a planar layout (multi-plane in
 *  buf->datas[]). PipeWire's typical convention for these formats is
 *  to put all planes in datas[0] sequentially, but multi-plane
 *  producers may split. */
inline bool isPlanar(Tag t) noexcept
{
  switch(t)
  {
    case Tag::YUV420P:
    case Tag::YV12:
    case Tag::NV12:
    case Tag::P010:
    case Tag::P210:
      return true;
    default:
      return false;
  }
}

/** DRM fourcc for the format, used when wrapping pipewire DmaBuf as
 *  an AVDRMFrameDescriptor. Returns 0 for unsupported formats.
 *
 *  Delegates to the shared interop table (Gfx/Graph/interop/DrmFourcc.hpp).
 *  Note the 10-bit mapping changed with the unification: RGB10A2 (SPA
 *  xRGB_210LE, R in bits 29-20) is DRM_FORMAT_ARGB2101010 'AR30' per the
 *  DRM spec — the previous 'AB30' value here was the mirrored layout. */
inline uint32_t toDrmFourcc(Tag t) noexcept
{
  // P210 has no SPA enum (toSpa aliases it to P010), so map it directly.
  if(t == Tag::P210)
    return score::gfx::interop::DRM_P210;
  return score::gfx::interop::spaToDrmFourcc(toSpa(t));
}

} // namespace Gfx::PipeWire::formats

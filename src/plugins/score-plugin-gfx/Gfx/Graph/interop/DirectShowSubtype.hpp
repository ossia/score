#pragma once

/**
 * @file DirectShowSubtype.hpp
 * @brief The DirectShow enumeration decision, expressed without windows.h.
 *
 * `CameraDevice.win32.cpp` reduces a `MEDIASUBTYPE_*` GUID to the fourcc in its
 * `Data1` field, resolves that through the shared table, and chooses a codec.
 * All three steps are pure arithmetic over the GUID's bytes, so they live here,
 * over a layout-identical POD, and are exercised on every host rather than only
 * where the enumeration itself can be compiled.
 *
 * Deliberately not platform-guarded: the point is that it builds everywhere.
 */

#include <Gfx/Graph/interop/DirectShowPixelFormat.hpp>
#include <Gfx/Graph/interop/VideoPixelFormatAV.hpp>

#include <cstdint>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/pixfmt.h>
}

namespace score::gfx::interop
{

/// Layout-identical to the Win32 `GUID`, so a `const GUID&` can be read through
/// a reference to this on the platform where one exists.
struct DirectShowGuid
{
  uint32_t Data1;
  uint16_t Data2;
  uint16_t Data3;
  uint8_t Data4[8];
};

/// Every YUV MEDIASUBTYPE is
/// {fourcc, 0x0000, 0x0010, {0x80,0,0,0xaa,0,0x38,0x9b,0x71}}, so the subtype
/// reduces to the fourcc in Data1. Returns 0 for the RGB subtypes, which are
/// genuine SDK GUIDs and are matched by name instead.
constexpr uint32_t directShowSubtypeFourcc(const DirectShowGuid& subtype) noexcept
{
  constexpr uint8_t suffix[8] = {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};
  if(subtype.Data2 != 0x0000 || subtype.Data3 != 0x0010)
    return 0;
  for(int i = 0; i < 8; ++i)
    if(subtype.Data4[i] != suffix[i])
      return 0;
  return subtype.Data1;
}

/// True when the subtype names a compressed stream rather than a raw layout.
inline bool directShowSubtypeIsCompressed(const DirectShowGuid& subtype) noexcept
{
  const auto fourcc = directShowSubtypeFourcc(subtype);
  return fourcc != 0 && isDirectShowCompressedFourcc(fourcc);
}

/// The AVPixelFormat behind a YUV subtype, or AV_PIX_FMT_NONE when the subtype
/// is not a fourcc one. The V-before-U layouts have no AVPixelFormat of their
/// own; their twin is named so the format stays offered, and
/// chromaSwappedTwin() records that a consumer wanting correct chroma must
/// exchange the U and V planes.
inline AVPixelFormat
directShowSubtypePixelFormat(const DirectShowGuid& subtype) noexcept
{
  const auto fourcc = directShowSubtypeFourcc(subtype);
  if(fourcc == 0)
    return AV_PIX_FMT_NONE;

  const auto layout = fromDirectShowFourcc(fourcc);
  if(layout == VideoPixelFormat::Unknown)
    return AV_PIX_FMT_NONE;

  if(const auto av = toAVPixelFormat(layout); av != AV_PIX_FMT_NONE)
    return av;
  return toAVPixelFormat(chromaSwappedTwin(layout));
}

/// The codec `enumerateCameraFormat` offers for a subtype.
///
/// A compressed fourcc dispatches to the codec it actually names — H.264 to
/// AV_CODEC_ID_H264, the DV spelling to DV, the Motion-JPEG spellings to
/// MJPEG — rather than collapsing everything to MJPEG, which offered an
/// H.264 camera with the wrong decoder. A non-compressed subtype is raw.
inline AVCodecID directShowSubtypeCodec(const DirectShowGuid& subtype) noexcept
{
  const auto fourcc = directShowSubtypeFourcc(subtype);
  if(fourcc == 0 || !isDirectShowCompressedFourcc(fourcc))
    return AV_CODEC_ID_RAWVIDEO;

  if(fourcc == directShowFourcc('H', '2', '6', '4'))
    return AV_CODEC_ID_H264;
  if(fourcc == directShowFourcc('d', 'v', 's', 'd'))
    return AV_CODEC_ID_DVVIDEO;
  // MJPG and the historical Motion-JPEG spellings.
  return AV_CODEC_ID_MJPEG;
}

} // namespace score::gfx::interop

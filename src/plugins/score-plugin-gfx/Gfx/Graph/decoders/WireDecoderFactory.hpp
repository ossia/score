#pragma once

/**
 * @file WireDecoderFactory.hpp
 * @brief Vendor-neutral GPU-decoder selection keyed on VideoPixelFormat.
 *
 * The capture-side symmetric counterpart to encoders/WireEncoderFactory.hpp:
 * turns "the on-wire pixel format a card delivers" into the GPUVideoDecoder that
 * unpacks those bytes into RGBA at sample time. Every capture-card addon
 * (AJA, and future DeckLink/Bluefish/Magewell/Deltacast) used to hand-roll the
 * same switch in its DMACaptureBackend::makeDecoder; this centralises it so an
 * addon only maps its vendor enum -> VideoPixelFormat.
 *
 * Returns nullptr for formats with no capture decoder yet. The decoder
 * allocates an input texture sized to the wire byte layout (the strategy DMAs
 * into it); `meta` carries the VPID/InfoFrame-derived colour metadata.
 */

#include <Gfx/Graph/decoders/Bayer.hpp>
#include <Gfx/Graph/decoders/NV12.hpp>
#include <Gfx/Graph/decoders/PackedBitfield.hpp>
#include <Gfx/Graph/decoders/PackedBitfieldYUV.hpp>
#include <Gfx/Graph/decoders/NV16.hpp>
#include <Gfx/Graph/decoders/NV24.hpp>
#include <Gfx/Graph/decoders/P010.hpp>
#include <Gfx/Graph/decoders/R210.hpp>
#include <Gfx/Graph/decoders/RGBA.hpp>
#include <Gfx/Graph/decoders/VUYA.hpp>
#include <Gfx/Graph/decoders/YUV420.hpp>
#include <Gfx/Graph/decoders/YUV422.hpp>
#include <Gfx/Graph/decoders/V210.hpp>
#include <Gfx/Graph/decoders/YUV422P10.hpp>
#include <Gfx/Graph/decoders/YUV444.hpp>
#include <Gfx/Graph/decoders/YUV444P12.hpp>
#include <Gfx/Graph/decoders/YUV444P10.hpp>
#include <Gfx/Graph/decoders/YUV422P12.hpp>
#include <Gfx/Graph/decoders/YUV420P10.hpp>
#include <Gfx/Graph/decoders/P210.hpp>
#include <Gfx/Graph/decoders/YUYV422.hpp>
#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <Video/VideoInterface.hpp>

#include <memory>

namespace score::gfx
{

inline std::unique_ptr<GPUVideoDecoder>
makeWireDecoder(score::gfx::interop::VideoPixelFormat fmt, Video::ImageFormat& d)
{
  using F = score::gfx::interop::VideoPixelFormat;
  switch(fmt)
  {
    // -- packed 8-bit YUV 4:2:2 --
    case F::UYVY422:
      return std::make_unique<UYVY422Decoder>(d);
    case F::YUYV422:
      return std::make_unique<YUYV422Decoder>(d);
    // Chroma-swapped twins: same unpack, the two UV taps exchanged.
    case F::YVYU422:
      return std::make_unique<YUYV422Decoder>(d, /*swapChroma=*/true);
    case F::VYUY422:
      return std::make_unique<UYVY422Decoder>(d, /*swapChroma=*/true);

    // -- packed 10-bit YUV 4:2:2 --
    case F::V210:
      return std::make_unique<V210Decoder>(d);

    // -- packed 8-bit RGB (the input texture stores these byte orders) --
    case F::BGRA8: // memory [B,G,R,A]
      return std::make_unique<PackedDecoder>(QRhiTexture::BGRA8, 4, d);
    case F::RGBA8: // memory [R,G,B,A]
      return std::make_unique<PackedDecoder>(QRhiTexture::RGBA8, 4, d);
    // The remaining 32-bit orderings are the same bytes read differently.
    // QRhiTexture::BGRA8 describes the UPLOAD byte order -- the sampler still
    // yields .r == red -- so a texture whose memory is B,G,R,A needs no
    // swizzle, and the others below are expressed relative to that. The X
    // variants additionally pin alpha, whose byte is padding and would
    // otherwise sample as transparent.
    case F::BGRX8:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::BGRA8, 4, d, "processed.rgba = vec4(tex.rgb, 1.0);");
    case F::RGBX8:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, d, "processed.rgba = vec4(tex.rgb, 1.0);");
    case F::ARGB8: // memory [A,R,G,B]
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, d, "processed.rgba = vec4(tex.gba, tex.r);");
    case F::XRGB8:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, d, "processed.rgba = vec4(tex.gba, 1.0);");
    case F::ABGR8: // memory [A,B,G,R]
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, d, "processed.rgba = vec4(tex.abg, tex.r);");
    case F::XBGR8:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, d, "processed.rgba = vec4(tex.abg, 1.0);");

    // -- single-channel / greyscale --
    // Machine-vision and depth sensors deliver one channel per pixel: V4L2
    // GREY, RealSense Z16, industrial Mono8/10/12/16. Same texture formats and
    // swizzle GPUVideoDecoderFactory already uses for AV_PIX_FMT_GRAY8/GRAY16 --
    // this factory maps the neutral WIRE enum rather than AVPixelFormat, so the
    // case is needed here too, but the decoder behind it is the shared one.
    // 10- and 12-bit mono travel in a 16-bit container and share the R16 path.
    case F::Mono8:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::R8, 1, d,
          "processed.rgba = vec4(tex.r, tex.r, tex.r, 1.0);");
    // 10- and 12-bit mono ride in a 16-bit lane, so the sampler normalises
    // against 65535 and the image comes out 64x / 16x too dark. Scale back to
    // full range; Mono16 already fills the lane.
    case F::Mono10:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::R16, 2, d,
          "processed.rgba = vec4(vec3(tex.r * 64.0625), 1.0);");
    case F::Mono12:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::R16, 2, d,
          "processed.rgba = vec4(vec3(tex.r * 16.0039), 1.0);");
    case F::Mono16:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::R16, 2, d,
          "processed.rgba = vec4(tex.r, tex.r, tex.r, 1.0);");
    // -- Bayer: one sample per pixel, demosaiced on the GPU ---------------
    // The CFA order travels with the format, so it is passed to the decoder
    // rather than assumed. The 10- and 12-bit orders ride right-aligned in a
    // 16-bit lane, so they carry the same rescale as Mono10 / Mono12.
    case F::BayerRGGB8:
    case F::BayerRG8: // PFNC spelling of the same order
      return std::make_unique<BayerDecoder>(
          QRhiTexture::R8, 1, d, BayerDecoder::Phase::RGGB);
    case F::BayerBGGR8:
      return std::make_unique<BayerDecoder>(
          QRhiTexture::R8, 1, d, BayerDecoder::Phase::BGGR);
    case F::BayerGRBG8:
      return std::make_unique<BayerDecoder>(
          QRhiTexture::R8, 1, d, BayerDecoder::Phase::GRBG);
    case F::BayerGBRG8:
      return std::make_unique<BayerDecoder>(
          QRhiTexture::R8, 1, d, BayerDecoder::Phase::GBRG);
    case F::BayerRGGB16:
      return std::make_unique<BayerDecoder>(
          QRhiTexture::R16, 2, d, BayerDecoder::Phase::RGGB);
    case F::BayerBGGR16:
      return std::make_unique<BayerDecoder>(
          QRhiTexture::R16, 2, d, BayerDecoder::Phase::BGGR);
    case F::BayerRGGB10:
      return std::make_unique<BayerDecoder>(
          QRhiTexture::R16, 2, d, BayerDecoder::Phase::RGGB, 64.0625);
    case F::BayerBGGR10:
      return std::make_unique<BayerDecoder>(
          QRhiTexture::R16, 2, d, BayerDecoder::Phase::BGGR, 64.0625);
    case F::BayerGRBG10:
      return std::make_unique<BayerDecoder>(
          QRhiTexture::R16, 2, d, BayerDecoder::Phase::GRBG, 64.0625);
    case F::BayerGBRG10:
      return std::make_unique<BayerDecoder>(
          QRhiTexture::R16, 2, d, BayerDecoder::Phase::GBRG, 64.0625);
    case F::BayerRG12:
      return std::make_unique<BayerDecoder>(
          QRhiTexture::R16, 2, d, BayerDecoder::Phase::RGGB, 16.0039);

    // Big-endian: sample the two bytes separately and reassemble, since R16
    // would read them in the host's order.
    case F::Mono16BE:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RG8, 2, d,
          "float be = (tex.r * 255.0 * 256.0 + tex.g * 255.0) / 65535.0;"
          "processed.rgba = vec4(be, be, be, 1.0);",
          /*invertY=*/false, /*nearest=*/true);

    // -- sub-byte packed RGB ---------------------------------------------
    // One parameterised unpacker; each format is just its bit layout. Bit
    // offsets are LSB-relative in the reassembled container, matching how
    // V4L2 documents them.
    case F::RGB332: // one byte: RRRGGGBB
      return makePackedBitfieldDecoder(
          {.containerBytes = 1, .bigEndian = false,
           .r = {5, 3}, .g = {2, 3}, .b = {0, 2}, .a = {0, 0}}, d);
    case F::RGB565:
      return makePackedBitfieldDecoder(
          {.containerBytes = 2, .bigEndian = false,
           .r = {11, 5}, .g = {5, 6}, .b = {0, 5}, .a = {0, 0}}, d);
    case F::RGB565BE:
      return makePackedBitfieldDecoder(
          {.containerBytes = 2, .bigEndian = true,
           .r = {11, 5}, .g = {5, 6}, .b = {0, 5}, .a = {0, 0}}, d);
    case F::RGB555:
      return makePackedBitfieldDecoder(
          {.containerBytes = 2, .bigEndian = false,
           .r = {10, 5}, .g = {5, 5}, .b = {0, 5}, .a = {0, 0}}, d);
    case F::RGB555BE:
      return makePackedBitfieldDecoder(
          {.containerBytes = 2, .bigEndian = true,
           .r = {10, 5}, .g = {5, 5}, .b = {0, 5}, .a = {0, 0}}, d);
    case F::ARGB1555:
      return makePackedBitfieldDecoder(
          {.containerBytes = 2, .bigEndian = false,
           .r = {10, 5}, .g = {5, 5}, .b = {0, 5}, .a = {15, 1}}, d);
    case F::RGB444:
      return makePackedBitfieldDecoder(
          {.containerBytes = 2, .bigEndian = false,
           .r = {8, 4}, .g = {4, 4}, .b = {0, 4}, .a = {0, 0}}, d);
    case F::ARGB4444:
      return makePackedBitfieldDecoder(
          {.containerBytes = 2, .bigEndian = false,
           .r = {8, 4}, .g = {4, 4}, .b = {0, 4}, .a = {12, 4}}, d);

    // Same containers as the RGB bitfields above, carrying Y/U/V. V4L2
    // documents them as "A/XYUV": alpha high, then Y, U, V downwards.
    // The high bits are NOT alpha: V4L2 documents them as "undefined when
    // reading from the driver" for the sub-8bpc packed YUV formats, unlike the
    // 8bpc AYUV32/VUYA32 family whose alpha is meaningful. Consuming them
    // rendered the frame fully transparent whenever a driver left them zero.
    case F::AYUV4444:
      return std::make_unique<PackedBitfieldYUVDecoder>(
          d, PackedBitfieldYUVLayout{
                 .y = {8, 4}, .u = {4, 4}, .v = {0, 4}, .a = {0, 0}});
    case F::AYUV1555:
      return std::make_unique<PackedBitfieldYUVDecoder>(
          d, PackedBitfieldYUVLayout{
                 .y = {10, 5}, .u = {5, 5}, .v = {0, 5}, .a = {0, 0}});
    case F::YUV565:
      return std::make_unique<PackedBitfieldYUVDecoder>(
          d, PackedBitfieldYUVLayout{
                 .y = {11, 5}, .u = {5, 6}, .v = {0, 5}, .a = {0, 0}});

    // -- packed 10-bit RGB (r210, DeckLink SDI 4:4:4 wire) --
    case F::R210:
      return std::make_unique<R210Decoder>(d);

    // -- packed RGB 24/48-bit --
    case F::RGB24:
      return std::make_unique<RGB24Decoder>(d);
    case F::BGR24:
      // Same unpacker, opposite byte order -- without the swizzle this
      // rendered R and B exchanged. GPUVideoDecoderFactory passes the same
      // filter for AV_PIX_FMT_BGR24.
      return std::make_unique<RGB24Decoder>(
          d, "processed.rgb = tex.bgr;");
    case F::RGB48:
      return std::make_unique<RGB48Decoder>(d);

    // -- planar / semi-planar (capture cards that convert on-chip, e.g. Magewell) --
    case F::NV12:
      return std::make_unique<NV12Decoder>(d, /*inverted=*/false);
    case F::NV21:
      return std::make_unique<NV12Decoder>(d, /*inverted=*/true);
    case F::NV16:
      return std::make_unique<NV16Decoder>(d, /*swapChroma=*/false);
    case F::NV61:
      return std::make_unique<NV16Decoder>(d, /*swapChroma=*/true);
    case F::NV24:
      return std::make_unique<NV24Decoder>(d, /*inverted=*/false);
    case F::NV42:
      return std::make_unique<NV24Decoder>(d, /*inverted=*/true);
    case F::VUYA:
      return std::make_unique<VUYADecoder>(d, /*opaque=*/false);
    case F::VUYX:
      return std::make_unique<VUYADecoder>(d, /*opaque=*/true);
    // Same 32 bits, different component order.
    case F::AYUV:
      return std::make_unique<VUYADecoder>(d, /*opaque=*/false, "gba", 'r');
    case F::XYUV:
      return std::make_unique<VUYADecoder>(d, /*opaque=*/true, "gba", 'r');
    case F::YUVA:
      return std::make_unique<VUYADecoder>(d, /*opaque=*/false, "rgb", 'a');
    case F::YUVX:
      return std::make_unique<VUYADecoder>(d, /*opaque=*/true, "rgb", 'a');
    case F::YUV420P:
      return std::make_unique<YUV420Decoder>(d, /*swapPlanes=*/false);
    case F::YVU420P:
      return std::make_unique<YUV420Decoder>(d, /*swapPlanes=*/true);
    case F::YUV422P:
      return std::make_unique<YUV422Decoder>(d);
    case F::P010:
      return std::make_unique<P010Decoder>(d);
    case F::YUV422P10:
      return std::make_unique<YUV422P10Decoder>(d);
    // The remaining planar decoders. Each already existed and was already
    // exercised through the ffmpeg path; only the wire mapping was missing, so
    // a card announcing one of these fourccs was refused at open().
    case F::YUV422P12:
      return std::make_unique<YUV422P12Decoder>(d);
    case F::YUV420P10:
      return std::make_unique<YUV420P10Decoder>(d);
    case F::YUV444P:
      return std::make_unique<YUV444Decoder>(d);
    case F::YUV444P10:
      return std::make_unique<YUV444P10Decoder>(d);
    case F::YUV444P12:
      return std::make_unique<YUV444P12Decoder>(d);
    case F::P210:
      return std::make_unique<P210Decoder>(d);

    default:
      return nullptr;
  }
}

} // namespace score::gfx

#pragma once
#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV
#include <ossia/detail/hash_map.hpp>

#include <QDebug>

#include <score_plugin_media_export.h>

#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

namespace Video
{

inline const ossia::hash_map<std::string, AVPixelFormat>& gstreamerToLibav()
{
  static const auto map = [] {
    ossia::hash_map<std::string, AVPixelFormat> format_map;

    /*
    * @GST_VIDEO_FORMAT_I420: planar 4:2:0 YUV
    * @GST_VIDEO_FORMAT_YV12: planar 4:2:0 YVU (like I420 but UV planes swapped)
    * @GST_VIDEO_FORMAT_YUY2: packed 4:2:2 YUV (Y0-U0-Y1-V0 Y2-U2-Y3-V2 Y4 ...)
    * @GST_VIDEO_FORMAT_UYVY: packed 4:2:2 YUV (U0-Y0-V0-Y1 U2-Y2-V2-Y3 U4 ...)
    * @GST_VIDEO_FORMAT_VYUY: packed 4:2:2 YUV (V0-Y0-U0-Y1 V2-Y2-U2-Y3 V4 ...)
    * @GST_VIDEO_FORMAT_AYUV: packed 4:4:4 YUV with alpha channel (A0-Y0-U0-V0 ...)
    * @GST_VIDEO_FORMAT_RGBx: sparse rgb packed into 32 bit, space last
    * @GST_VIDEO_FORMAT_BGRx: sparse reverse rgb packed into 32 bit, space last
    * @GST_VIDEO_FORMAT_xRGB: sparse rgb packed into 32 bit, space first
    * @GST_VIDEO_FORMAT_xBGR: sparse reverse rgb packed into 32 bit, space first
    * @GST_VIDEO_FORMAT_RGBA: rgb with alpha channel last
    * @GST_VIDEO_FORMAT_BGRA: reverse rgb with alpha channel last
    * @GST_VIDEO_FORMAT_ARGB: rgb with alpha channel first
    * @GST_VIDEO_FORMAT_ABGR: reverse rgb with alpha channel first
    * @GST_VIDEO_FORMAT_RGB: RGB packed into 24 bits without padding (`R-G-B-R-G-B`)
    * @GST_VIDEO_FORMAT_BGR: reverse RGB packed into 24 bits without padding (`B-G-R-B-G-R`)
    * @GST_VIDEO_FORMAT_Y41B: planar 4:1:1 YUV
    * @GST_VIDEO_FORMAT_Y42B: planar 4:2:2 YUV
    * @GST_VIDEO_FORMAT_YVYU: packed 4:2:2 YUV (Y0-V0-Y1-U0 Y2-V2-Y3-U2 Y4 ...)
    * @GST_VIDEO_FORMAT_Y444: planar 4:4:4 YUV
    * @GST_VIDEO_FORMAT_v210: packed 4:2:2 10-bit YUV, complex format
    * @GST_VIDEO_FORMAT_v216: packed 4:2:2 16-bit YUV, Y0-U0-Y1-V1 order
    * @GST_VIDEO_FORMAT_NV12: planar 4:2:0 YUV with interleaved UV plane
    * @GST_VIDEO_FORMAT_NV21: planar 4:2:0 YUV with interleaved VU plane
    * @GST_VIDEO_FORMAT_NV12_10LE32: 10-bit variant of @GST_VIDEO_FORMAT_NV12, packed into 32bit words (MSB 2 bits padding) (Since: 1.14)
    * @GST_VIDEO_FORMAT_GRAY8: 8-bit grayscale
    * @GST_VIDEO_FORMAT_GRAY10_LE32: 10-bit grayscale, packed into 32bit words (2 bits padding) (Since: 1.14)
    * @GST_VIDEO_FORMAT_GRAY16_BE: 16-bit grayscale, most significant byte first
    * @GST_VIDEO_FORMAT_GRAY16_LE: 16-bit grayscale, least significant byte first
    * @GST_VIDEO_FORMAT_v308: packed 4:4:4 YUV (Y-U-V ...)
    * @GST_VIDEO_FORMAT_IYU2: packed 4:4:4 YUV (U-Y-V ...) (Since: 1.10)
    * @GST_VIDEO_FORMAT_RGB16: rgb 5-6-5 bits per component
    * @GST_VIDEO_FORMAT_BGR16: reverse rgb 5-6-5 bits per component
    * @GST_VIDEO_FORMAT_RGB15: rgb 5-5-5 bits per component
    * @GST_VIDEO_FORMAT_BGR15: reverse rgb 5-5-5 bits per component
    * @GST_VIDEO_FORMAT_UYVP: packed 10-bit 4:2:2 YUV (U0-Y0-V0-Y1 U2-Y2-V2-Y3 U4 ...)
    * @GST_VIDEO_FORMAT_A420: planar 4:4:2:0 AYUV
    * @GST_VIDEO_FORMAT_RGB8P: 8-bit paletted RGB
    * @GST_VIDEO_FORMAT_YUV9: planar 4:1:0 YUV
    * @GST_VIDEO_FORMAT_YVU9: planar 4:1:0 YUV (like YUV9 but UV planes swapped)
    * @GST_VIDEO_FORMAT_IYU1: packed 4:1:1 YUV (Cb-Y0-Y1-Cr-Y2-Y3 ...)
    * @GST_VIDEO_FORMAT_ARGB64: rgb with alpha channel first, 16 bits per channel
    * @GST_VIDEO_FORMAT_AYUV64: packed 4:4:4 YUV with alpha channel, 16 bits per channel (A0-Y0-U0-V0 ...)
    * @GST_VIDEO_FORMAT_r210: packed 4:4:4 RGB, 10 bits per channel
    * @GST_VIDEO_FORMAT_I420_10BE: planar 4:2:0 YUV, 10 bits per channel
    * @GST_VIDEO_FORMAT_I420_10LE: planar 4:2:0 YUV, 10 bits per channel
    * @GST_VIDEO_FORMAT_I422_10BE: planar 4:2:2 YUV, 10 bits per channel
    * @GST_VIDEO_FORMAT_I422_10LE: planar 4:2:2 YUV, 10 bits per channel
    * @GST_VIDEO_FORMAT_Y444_10BE: planar 4:4:4 YUV, 10 bits per channel (Since: 1.2)
    * @GST_VIDEO_FORMAT_Y444_10LE: planar 4:4:4 YUV, 10 bits per channel (Since: 1.2)
    * @GST_VIDEO_FORMAT_GBR: planar 4:4:4 RGB, 8 bits per channel (Since: 1.2)
    * @GST_VIDEO_FORMAT_GBR_10BE: planar 4:4:4 RGB, 10 bits per channel (Since: 1.2)
    * @GST_VIDEO_FORMAT_GBR_10LE: planar 4:4:4 RGB, 10 bits per channel (Since: 1.2)
    * @GST_VIDEO_FORMAT_NV16: planar 4:2:2 YUV with interleaved UV plane (Since: 1.2)
    * @GST_VIDEO_FORMAT_NV16_10LE32: 10-bit variant of @GST_VIDEO_FORMAT_NV16, packed into 32bit words (MSB 2 bits padding) (Since: 1.14)
    * @GST_VIDEO_FORMAT_NV24: planar 4:4:4 YUV with interleaved UV plane (Since: 1.2)
    * @GST_VIDEO_FORMAT_NV12_64Z32: NV12 with 64x32 tiling in zigzag pattern (Since: 1.4)
    * @GST_VIDEO_FORMAT_A420_10BE: planar 4:4:2:0 YUV, 10 bits per channel (Since: 1.6)
    * @GST_VIDEO_FORMAT_A420_10LE: planar 4:4:2:0 YUV, 10 bits per channel (Since: 1.6)
    * @GST_VIDEO_FORMAT_A422_10BE: planar 4:4:2:2 YUV, 10 bits per channel (Since: 1.6)
    * @GST_VIDEO_FORMAT_A422_10LE: planar 4:4:2:2 YUV, 10 bits per channel (Since: 1.6)
    * @GST_VIDEO_FORMAT_A444_10BE: planar 4:4:4:4 YUV, 10 bits per channel (Since: 1.6)
    * @GST_VIDEO_FORMAT_A444_10LE: planar 4:4:4:4 YUV, 10 bits per channel (Since: 1.6)
    * @GST_VIDEO_FORMAT_NV61: planar 4:2:2 YUV with interleaved VU plane (Since: 1.6)
    * @GST_VIDEO_FORMAT_P010_10BE: planar 4:2:0 YUV with interleaved UV plane, 10 bits per channel (Since: 1.10)
    * @GST_VIDEO_FORMAT_P010_10LE: planar 4:2:0 YUV with interleaved UV plane, 10 bits per channel (Since: 1.10)
    * @GST_VIDEO_FORMAT_GBRA: planar 4:4:4:4 ARGB, 8 bits per channel (Since: 1.12)
    * @GST_VIDEO_FORMAT_GBRA_10BE: planar 4:4:4:4 ARGB, 10 bits per channel (Since: 1.12)
    * @GST_VIDEO_FORMAT_GBRA_10LE: planar 4:4:4:4 ARGB, 10 bits per channel (Since: 1.12)
    * @GST_VIDEO_FORMAT_GBR_12BE: planar 4:4:4 RGB, 12 bits per channel (Since: 1.12)
    * @GST_VIDEO_FORMAT_GBR_12LE: planar 4:4:4 RGB, 12 bits per channel (Since: 1.12)
    * @GST_VIDEO_FORMAT_GBRA_12BE: planar 4:4:4:4 ARGB, 12 bits per channel (Since: 1.12)
    * @GST_VIDEO_FORMAT_GBRA_12LE: planar 4:4:4:4 ARGB, 12 bits per channel (Since: 1.12)
    * @GST_VIDEO_FORMAT_I420_12BE: planar 4:2:0 YUV, 12 bits per channel (Since: 1.12)
    * @GST_VIDEO_FORMAT_I420_12LE: planar 4:2:0 YUV, 12 bits per channel (Since: 1.12)
    * @GST_VIDEO_FORMAT_I422_12BE: planar 4:2:2 YUV, 12 bits per channel (Since: 1.12)
    * @GST_VIDEO_FORMAT_I422_12LE: planar 4:2:2 YUV, 12 bits per channel (Since: 1.12)
    * @GST_VIDEO_FORMAT_Y444_12BE: planar 4:4:4 YUV, 12 bits per channel (Since: 1.12)
    * @GST_VIDEO_FORMAT_Y444_12LE: planar 4:4:4 YUV, 12 bits per channel (Since: 1.12)
    * @GST_VIDEO_FORMAT_NV12_10LE40: Fully packed variant of NV12_10LE32 (Since: 1.16)
    * @GST_VIDEO_FORMAT_Y210: packed 4:2:2 YUV, 10 bits per channel (Since: 1.16)
    * @GST_VIDEO_FORMAT_Y410: packed 4:4:4 YUV, 10 bits per channel(A-V-Y-U...) (Since: 1.16)
    * @GST_VIDEO_FORMAT_VUYA: packed 4:4:4 YUV with alpha channel (V0-U0-Y0-A0...) (Since: 1.16)
    * @GST_VIDEO_FORMAT_BGR10A2_LE: packed 4:4:4 RGB with alpha channel(B-G-R-A), 10 bits for R/G/B channel and MSB 2 bits for alpha channel (Since: 1.16)
    * @GST_VIDEO_FORMAT_RGB10A2_LE: packed 4:4:4 RGB with alpha channel(R-G-B-A), 10 bits for R/G/B channel and MSB 2 bits for alpha channel (Since: 1.18)
    * @GST_VIDEO_FORMAT_Y444_16BE: planar 4:4:4 YUV, 16 bits per channel (Since: 1.18)
    * @GST_VIDEO_FORMAT_Y444_16LE: planar 4:4:4 YUV, 16 bits per channel (Since: 1.18)
    * @GST_VIDEO_FORMAT_P016_BE: planar 4:2:0 YUV with interleaved UV plane, 16 bits per channel (Since: 1.18)
    * @GST_VIDEO_FORMAT_P016_LE: planar 4:2:0 YUV with interleaved UV plane, 16 bits per channel (Since: 1.18)
    * @GST_VIDEO_FORMAT_P012_BE: planar 4:2:0 YUV with interleaved UV plane, 12 bits per channel (Since: 1.18)
    * @GST_VIDEO_FORMAT_P012_LE: planar 4:2:0 YUV with interleaved UV plane, 12 bits per channel (Since: 1.18)
    * @GST_VIDEO_FORMAT_Y212_BE: packed 4:2:2 YUV, 12 bits per channel (Y-U-Y-V) (Since: 1.18)
    * @GST_VIDEO_FORMAT_Y212_LE: packed 4:2:2 YUV, 12 bits per channel (Y-U-Y-V) (Since: 1.18)
    * @GST_VIDEO_FORMAT_Y412_BE: packed 4:4:4:4 YUV, 12 bits per channel(U-Y-V-A...) (Since: 1.18)
    * @GST_VIDEO_FORMAT_Y412_LE: packed 4:4:4:4 YUV, 12 bits per channel(U-Y-V-A...) (Since: 1.18)
    * @GST_VIDEO_FORMAT_NV12_4L4: NV12 with 4x4 tiles in linear order (Since: 1.18)
    * @GST_VIDEO_FORMAT_NV12_32L32: NV12 with 32x32 tiles in linear order (Since: 1.18)
    * @GST_VIDEO_FORMAT_RGBP: planar 4:4:4 RGB, 8 bits per channel (Since: 1.20)
    * @GST_VIDEO_FORMAT_BGRP: planar 4:4:4 RGB, 8 bits per channel (Since: 1.20)
    * @GST_VIDEO_FORMAT_AV12: Planar 4:2:0 YUV with interleaved UV plane with alpha as 3rd plane (Since: 1.20)
    */

    format_map["A420"] = AV_PIX_FMT_YUVA420P;
    format_map["A420_10BE"] = AV_PIX_FMT_YUVA420P10BE;
    format_map["A420_10LE"] = AV_PIX_FMT_YUVA420P10LE;
    format_map["A422_10BE"] = AV_PIX_FMT_YUVA422P10BE;
    format_map["A422_10LE"] = AV_PIX_FMT_YUVA422P10LE;
    format_map["A444_10BE"] = AV_PIX_FMT_YUVA444P10BE;
    format_map["A444_10LE"] = AV_PIX_FMT_YUVA444P10LE;
    format_map["ABGR"] = AV_PIX_FMT_ABGR;
    // format_map["ABGR64_BE"] = AV_PIX_FMT_ABGR64BE;
    // format_map["ABGR64_LE"] = AV_PIX_FMT_ABGR64LE;
    format_map["ARGB"] = AV_PIX_FMT_ARGB;
    // format_map["ARGB64"] = AV_PIX_FMT_ARGB64;
    // format_map["ARGB64_BE"] = AV_PIX_FMT_ARGB64BE;
    // format_map["ARGB64_LE"] = AV_PIX_FMT_ARGB64LE;
    //  format_map["AV12"] = AV_PIX_FMT_AV12;
    //  format_map["AYUV"] = AV_PIX_FMT_AYUV;
    format_map["AYUV64"] = AV_PIX_FMT_AYUV64;
    // format_map["BGR"] = AV_PIX_FMT_BGR;
    // format_map["BGR10A2_LE"] = AV_PIX_FMT_BGR10A2LE;
    // format_map["BGR15"] = AV_PIX_FMT_BGR15;
    // format_map["BGR16"] = AV_PIX_FMT_BGR16;
    format_map["BGRA"] = AV_PIX_FMT_BGRA;
    // format_map["BGRA64_BE"] = AV_PIX_FMT_BGRA64BE;
    // format_map["BGRA64_LE"] = AV_PIX_FMT_BGRA64LE;
    // format_map["BGRP"] = AV_PIX_FMT_BGRP;
    format_map["BGRX"] = AV_PIX_FMT_BGR0;
    // format_map["ENCODED"] = AV_PIX_FMT_ENCODED;
    format_map["GBR"] = AV_PIX_FMT_GBRP;
    format_map["GBRA"] = AV_PIX_FMT_GBRAP;
    // format_map["GBRA_10BE"] = AV_PIX_FMT_GBRA_10BE;
    // format_map["GBRA_10LE"] = AV_PIX_FMT_GBRA_10LE;
    // format_map["GBRA_12BE"] = AV_PIX_FMT_GBRA_12BE;
    // format_map["GBRA_12LE"] = AV_PIX_FMT_GBRA_12LE;
    // format_map["GBR_10BE"] = AV_PIX_FMT_GBR_10BE;
    // format_map["GBR_10LE"] = AV_PIX_FMT_GBR_10LE;
    // format_map["GBR_12BE"] = AV_PIX_FMT_GBR_12BE;
    // format_map["GBR_12LE"] = AV_PIX_FMT_GBR_12LE;
    // format_map["GRAY10_LE32"] = AV_PIX_FMT_GRAY10_LE32;
    format_map["GRAY16_BE"] = AV_PIX_FMT_GRAY16BE;
    format_map["GRAY16_LE"] = AV_PIX_FMT_GRAY16LE;
    format_map["GRAY8"] = AV_PIX_FMT_GRAY8;
    format_map["I420"] = AV_PIX_FMT_YUV420P;
    format_map["I420_10BE"] = AV_PIX_FMT_YUV420P10BE;
    format_map["I420_10LE"] = AV_PIX_FMT_YUV420P10LE;
    format_map["I420_12BE"] = AV_PIX_FMT_YUV420P12BE;
    format_map["I420_12LE"] = AV_PIX_FMT_YUV420P12LE;
    format_map["I422_10BE"] = AV_PIX_FMT_YUV422P10BE;
    format_map["I422_10LE"] = AV_PIX_FMT_YUV422P10LE;
    format_map["I422_12BE"] = AV_PIX_FMT_YUV422P12BE;
    format_map["I422_12LE"] = AV_PIX_FMT_YUV422P12LE;
    // format_map["IYU1"] = AV_PIX_FMT_IYU1;
    // format_map["IYU2"] = AV_PIX_FMT_IYU2;
    format_map["NV12"] = AV_PIX_FMT_NV12;
    // format_map["NV12_10LE32"] = AV_PIX_FMT_NV12_10LE32;
    // format_map["NV12_10LE40"] = AV_PIX_FMT_NV12_10LE40;
    // format_map["NV12_32L32"] = AV_PIX_FMT_NV12_32L32;
    // format_map["NV12_4L4"] = AV_PIX_FMT_NV12_4L4;
    // format_map["NV12_64Z32"] = AV_PIX_FMT_NV12_64Z32;
    format_map["NV16"] = AV_PIX_FMT_NV16;
    // format_map["NV16_10LE32"] = AV_PIX_FMT_NV16_10LE32;
    format_map["NV21"] = AV_PIX_FMT_NV21;
    format_map["NV24"] = AV_PIX_FMT_NV24;
    // format_map["NV61"] = AV_PIX_FMT_NV61;
    format_map["P010_10BE"] = AV_PIX_FMT_P010BE;
    format_map["P010_10LE"] = AV_PIX_FMT_P010LE;
    // format_map["P012_BE"] = AV_PIX_FMT_P012BE;
    // format_map["P012_LE"] = AV_PIX_FMT_P012LE;
    format_map["P016_BE"] = AV_PIX_FMT_P016BE;
    format_map["P016_LE"] = AV_PIX_FMT_P016LE;
    // format_map["R210"] = AV_PIX_FMT_R210;
    format_map["RGB"] = AV_PIX_FMT_RGB24;
    // format_map["RGB10A2_LE"] = AV_PIX_FMT_RGB10A2LE;
    // format_map["RGB15"] = AV_PIX_FMT_RGB15;
    // format_map["RGB16"] = AV_PIX_FMT_RGB16;
    // format_map["RGB8P"] = AV_PIX_FMT_RGB8P;
    format_map["RGBA"] = AV_PIX_FMT_RGBA;
    format_map["RGBA64_BE"] = AV_PIX_FMT_RGBA64BE;
    format_map["RGBA64_LE"] = AV_PIX_FMT_RGBA64LE;
    format_map["RGBP"] = AV_PIX_FMT_RGB24;
    format_map["RGBX"] = AV_PIX_FMT_RGB0;
    // format_map["UNKNOWN"] = AV_PIX_FMT_UNKNOWN;
    // format_map["UYVP"] = AV_PIX_FMT_UYVP;
    format_map["UYVY"] = AV_PIX_FMT_UYVY422;
    // format_map["V210"] = AV_PIX_FMT_V210;
    // format_map["V216"] = AV_PIX_FMT_V216;
    // format_map["V308"] = AV_PIX_FMT_V308;
    // format_map["VUYA"] = AV_PIX_FMT_VUYA;
    // format_map["VYUY"] = AV_PIX_FMT_VYUY;
    format_map["XBGR"] = AV_PIX_FMT_0BGR;
    format_map["XRGB"] = AV_PIX_FMT_0RGB;
    format_map["Y210"] = AV_PIX_FMT_Y210;
    // format_map["Y212_BE"] = AV_PIX_FMT_Y212BE;
    // format_map["Y212_LE"] = AV_PIX_FMT_Y212LE;
    format_map["Y410"] = AV_PIX_FMT_YUV410P;
    // format_map["Y412_BE"] = AV_PIX_FMT_Y412BE;
    // format_map["Y412_LE"] = AV_PIX_FMT_Y412LE;
    // format_map["Y41B"] = AV_PIX_FMT_Y41B;
    // format_map["Y42B"] = AV_PIX_FMT_Y42B;
    format_map["Y444"] = AV_PIX_FMT_YUV444P;
    format_map["Y444_10BE"] = AV_PIX_FMT_YUV444P10BE;
    format_map["Y444_10LE"] = AV_PIX_FMT_YUV444P10LE;
    format_map["Y444_12BE"] = AV_PIX_FMT_YUV444P12BE;
    format_map["Y444_12LE"] = AV_PIX_FMT_YUV444P12LE;
    format_map["Y444_16BE"] = AV_PIX_FMT_YUV444P16BE;
    format_map["Y444_16LE"] = AV_PIX_FMT_YUV444P16LE;
    // format_map["YUV9"] = AV_PIX_FMT_YUV9;
    format_map["YUY2"] = AV_PIX_FMT_YUYV422;
    // format_map["YV12"] = AV_PIX_FMT_YV12;
    // format_map["YVU9"] = AV_PIX_FMT_YVU9;
    format_map["YVYU"] = AV_PIX_FMT_YVYU422;

    return format_map;
  }();

  return map;
}

// Lays out the planes of `frame` over a flat buffer of `sz` bytes, for the
// input devices that hand us pixels with no stride information at all.
//
// `align` is the row alignment the producer used: 1 for a tightly packed
// buffer, 4 for the row padding GStreamer applies in its default layout.
// libav owns every stride and every plane offset here -- av_image_fill_arrays()
// rounds subsampled planes UP, which is what a frame of odd width needs, and
// knows the plane count of the semi-planar and packed families.
[[nodiscard]]
inline bool
initFrameFromRawData(AVFrame* frame, uint8_t* p, std::size_t sz, int align)
{
  const auto fmt = (AVPixelFormat)frame->format;
  const int needed
      = av_image_get_buffer_size(fmt, frame->width, frame->height, align);
  if(needed < 0 || std::size_t(needed) > sz)
  {
    qDebug() << "unhandled video format" << av_get_pix_fmt_name(fmt) << ":" << sz
             << "bytes for a buffer that needs" << needed;
    return false;
  }

  if(av_image_fill_arrays(
         frame->data, frame->linesize, p, fmt, frame->width, frame->height, align)
     < 0)
  {
    qDebug() << "could not lay out video format" << av_get_pix_fmt_name(fmt);
    return false;
  }
  return true;
}

[[nodiscard]]
inline bool initFrameFromRawData(AVFrame* frame, uint8_t* p, std::size_t sz)
{
  return initFrameFromRawData(frame, p, sz, 1);
}

}
#endif

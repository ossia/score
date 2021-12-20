#pragma once
#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV
#include <string>
#include <unordered_map>
#include <score_plugin_media_export.h>
#include <QDebug>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

namespace Video
{

inline
const std::unordered_map<std::string, AVPixelFormat>& gstreamerToLibav()
{
  static const auto map = [] {
    std::unordered_map<std::string, AVPixelFormat> format_map;

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
    format_map["A444_10LE"] = AV_PIX_FMT_YUVA444P10BE;
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
    // format_map["YUY2"] = AV_PIX_FMT_YUY2;
    // format_map["YV12"] = AV_PIX_FMT_YV12;
    // format_map["YVU9"] = AV_PIX_FMT_YVU9;
    format_map["YVYU"] = AV_PIX_FMT_YVYU422;

    return format_map;
  }();

  return map;
}


// This is used when we do not have stride info, we make a best guess...
inline void initFrameFromRawData(AVFrame* frame, uint8_t* p, std::size_t sz)
{
  switch(frame->format)
  {
    case AV_PIX_FMT_YUV420P: ///< planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)
    case AV_PIX_FMT_YUVJ420P:  ///< planar YUV 4:2:0, 12bpp, full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV420P and setting color_range
    {
      // assuming 640x480:
      // first plane is 640x480 Y
      frame->data[0] = p;
      frame->linesize[0] = frame->width;

      // second plane is 320x240 U
      frame->data[1] = frame->data[0] + frame->linesize[0] * frame->height;
      frame->linesize[1] = frame->width / 2;

      // third plane is 320x240 U
      frame->data[2] = frame->data[1] + frame->linesize[1] * frame->height / 2;
      frame->linesize[2] = frame->width / 2;
      break;
    }

    case AV_PIX_FMT_NV12:      ///< planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components, which are interleaved (first byte U and the following byte V)
    case AV_PIX_FMT_NV21:      ///< Same but swapped
    {
      // assuming 640x480:
      // first plane is 640x480 Y
      frame->data[0] = p;
      frame->linesize[0] = frame->width;

      // second plane is 640x480 UV
      frame->data[1] = frame->data[0] + frame->linesize[0] * frame->height;
      frame->linesize[1] = frame->width;

      break;
    }

    case AV_PIX_FMT_P016LE: ///< like NV12: with 16bpp per component: little-endian
    case AV_PIX_FMT_P016BE: ///< like NV12: with 16bpp per component: big-endian
    {
      constexpr int byte_per_component = 2;
      // assuming 640x480:
      // first plane is 640x480 Y
      frame->data[0] = p;
      frame->linesize[0] = byte_per_component * frame->width;

      // second plane is 640x480 UV
      frame->data[1] = frame->data[0] + frame->linesize[0] * frame->height;
      frame->linesize[1] = byte_per_component * frame->width;

      break;
    }

    case AV_PIX_FMT_YUV420P16LE:  ///< planar YUV 4:2:0, 24bpp, (1 Cr & Cb sample per 2x2 Y samples), little-endian
    case AV_PIX_FMT_YUV420P16BE:  ///< planar YUV 4:2:0, 24bpp, (1 Cr & Cb sample per 2x2 Y samples), big-endian
    {
      constexpr int byte_per_component = 2;
      // assuming 640x480:
      // first plane is 640x480 Y
      frame->data[0] = p;
      frame->linesize[0] = byte_per_component * frame->width;

      // second plane is 320x240 U
      frame->data[1] = frame->data[0] + frame->linesize[0] * frame->height;
      frame->linesize[1] = byte_per_component * frame->width / 2;

      // third plane is 320x240 U
      frame->data[2] = frame->data[1] + frame->linesize[1] * frame->height / 2;
      frame->linesize[2] = byte_per_component * frame->width / 2;
      break;
    }

    case AV_PIX_FMT_YUV444P:  ///< planar YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples)
    case AV_PIX_FMT_YUVJ444P:  ///< planar YUV 4:4:4, 24bpp, full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV444P and setting color_range
    case AV_PIX_FMT_GBRP:      ///< planar GBR 4:4:4 24bpp
    {
      // assuming 640x480:
      // first plane is 640x480 Y
      frame->data[0] = p;
      frame->linesize[0] = frame->width;

      // second plane is 640x480 U
      frame->data[1] = frame->data[0] + frame->linesize[0] * frame->height;
      frame->linesize[1] = frame->width;

      // third plane is 640x480 V
      frame->data[2] = frame->data[1] + frame->linesize[1] * frame->height;
      frame->linesize[2] = frame->width;
      break;
    }


    case AV_PIX_FMT_YUV444P16LE:  ///< planar YUV 4:4:4, 48bpp, (1 Cr & Cb sample per 1x1 Y samples), little-endian
    case AV_PIX_FMT_YUV444P16BE:  ///< planar YUV 4:4:4, 48bpp, (1 Cr & Cb sample per 1x1 Y samples), big-endian
    {
      constexpr int byte_per_component = 2;
      // assuming 640x480:
      // first plane is 640x480 Y
      frame->data[0] = p;
      frame->linesize[0] = byte_per_component * frame->width;

      // second plane is 640x480 U
      frame->data[1] = frame->data[0] + frame->linesize[0] * frame->height;
      frame->linesize[1] = byte_per_component * frame->width;

      // third plane is 640x480 U
      frame->data[2] = frame->data[1] + frame->linesize[1] * frame->height;
      frame->linesize[2] = byte_per_component * frame->width;
      break;
    }

    case AV_PIX_FMT_YUVA444P:  ///< planar YUV 4:4:4 32bpp: (1 Cr & Cb sample per 1x1 Y & A samples)
    case AV_PIX_FMT_GBRAP:        ///< planar GBRA 4:4:4:4 32bpp
    case AV_PIX_FMT_GBRAP16BE:    ///< planar GBRA 4:4:4:4 64bpp: big-endian
    case AV_PIX_FMT_GBRAP16LE:    ///< planar GBRA 4:4:4:4 64bpp: little-endian
    {
      frame->data[0] = p;
      frame->linesize[0] = frame->width;

      frame->data[1] = frame->data[0] + frame->linesize[0] * frame->height;
      frame->linesize[1] = frame->width;

      frame->data[2] = frame->data[1] + frame->linesize[1] * frame->height;
      frame->linesize[2] = frame->width;

      frame->data[3] = frame->data[2] + frame->linesize[2] * frame->height;
      frame->linesize[3] = frame->width;
      break;
    }

    case AV_PIX_FMT_YUV422P:  ///< planar YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples)
    case AV_PIX_FMT_YUVJ422P:  ///< planar YUV 4:2:2, 16bpp, full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV422P and setting color_range
    {
      // assuming 640x480:
      // first plane is 640x480 Y
      frame->data[0] = p;
      frame->linesize[0] = frame->width;

      // second plane is 640x480 / 2 U
      frame->data[1] = frame->data[0] + frame->linesize[0] * frame->height;
      frame->linesize[1] = frame->width / 2;

      // third plane is 640x480 / 2 V
      frame->data[2] = frame->data[1] + frame->linesize[1] * frame->height;
      frame->linesize[2] = frame->width / 2;
      break;
    }

    case AV_PIX_FMT_YUV422P16LE:  ///< planar YUV 4:2:2, 32bpp, (1 Cr & Cb sample per 2x1 Y samples), little-endian
    case AV_PIX_FMT_YUV422P16BE:  ///< planar YUV 4:2:2, 32bpp, (1 Cr & Cb sample per 2x1 Y samples), big-endian
    {
      constexpr int byte_per_component = 2;
      // assuming 640x480:
      // first plane is 640x480 Y
      frame->data[0] = p;
      frame->linesize[0] = byte_per_component * frame->width;

      // second plane is 640x480 / 2 U
      frame->data[1] = frame->data[0] + frame->linesize[0] * frame->height;
      frame->linesize[1] = byte_per_component * frame->width / 2;

      // third plane is 3640x480 / 2 V
      frame->data[2] = frame->data[1] + frame->linesize[1] * frame->height;
      frame->linesize[2] = byte_per_component * frame->width / 2;
      break;
    }

    case AV_PIX_FMT_NV24:      ///< planar YUV 4:4:4: 24bpp: 1 plane for Y and 1 plane for the UV components: which are interleaved (first byte U and the following byte V)
    case AV_PIX_FMT_NV42:      ///< as above, but U and V bytes are swapped
    {
      // assuming 640x480:
      // first plane is 640x480 Y
      frame->data[0] = p;
      frame->linesize[0] = frame->width;

      // second plane is 640x480
      frame->data[1] = frame->data[0] + frame->linesize[0] * frame->height;
      frame->linesize[1] = frame->width * 2;
      break;
    }

    case AV_PIX_FMT_YUV410P:  ///< planar YUV 4:1:0,  9bpp, (1 Cr & Cb sample per 4x4 Y samples)
    {
      // assuming 640x480:
      // first plane is 640x480 Y
      frame->data[0] = p;
      frame->linesize[0] = frame->width;

      // second plane is 160x120 U
      frame->data[1] = frame->data[0] + frame->linesize[0] * frame->height;
      frame->linesize[1] = frame->width / 4;

      // third plane is 160x120 V
      frame->data[2] = frame->data[1] + frame->linesize[1] * frame->height / 4;
      frame->linesize[2] = frame->width / 4;
      break;
    }

    case AV_PIX_FMT_YUV411P:  ///< planar YUV 4:1:1, 12bpp, (1 Cr & Cb sample per 4x1 Y samples)
    {
      // assuming 640x480:
      // first plane is 640x480 Y
      frame->data[0] = p;
      frame->linesize[0] = frame->width;

      // second plane is 640x480 / 2 U
      frame->data[1] = frame->data[0] + frame->linesize[0] * frame->height;
      frame->linesize[1] = frame->width / 4;

      // third plane is 640x480 / 2 V
      frame->data[2] = frame->data[1] + frame->linesize[1] * frame->height;
      frame->linesize[2] = frame->width / 4;
      break;
    }

    case AV_PIX_FMT_YUV440P:   ///< planar YUV 4:4:0 (1 Cr & Cb sample per 1x2 Y samples)
    case AV_PIX_FMT_YUVJ440P:  ///< planar YUV 4:4:0 full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV440P and setting color_range
    case AV_PIX_FMT_YUVA420P:  ///< planar YUV 4:2:0, 20bpp, (1 Cr & Cb sample per 2x2 Y & A samples)

    case AV_PIX_FMT_YUV420P9BE: ///< planar YUV 4:2:0: 13.5bpp: (1 Cr & Cb sample per 2x2 Y samples): big-endian
    case AV_PIX_FMT_YUV420P9LE: ///< planar YUV 4:2:0: 13.5bpp: (1 Cr & Cb sample per 2x2 Y samples): little-endian
    case AV_PIX_FMT_YUV420P10BE:///< planar YUV 4:2:0: 15bpp: (1 Cr & Cb sample per 2x2 Y samples): big-endian
    case AV_PIX_FMT_YUV420P10LE:///< planar YUV 4:2:0: 15bpp: (1 Cr & Cb sample per 2x2 Y samples): little-endian
    case AV_PIX_FMT_YUV422P10BE:///< planar YUV 4:2:2: 20bpp: (1 Cr & Cb sample per 2x1 Y samples): big-endian
    case AV_PIX_FMT_YUV422P10LE:///< planar YUV 4:2:2: 20bpp: (1 Cr & Cb sample per 2x1 Y samples): little-endian
    case AV_PIX_FMT_YUV444P9BE: ///< planar YUV 4:4:4: 27bpp: (1 Cr & Cb sample per 1x1 Y samples): big-endian
    case AV_PIX_FMT_YUV444P9LE: ///< planar YUV 4:4:4: 27bpp: (1 Cr & Cb sample per 1x1 Y samples): little-endian
    case AV_PIX_FMT_YUV444P10BE:///< planar YUV 4:4:4: 30bpp: (1 Cr & Cb sample per 1x1 Y samples): big-endian
    case AV_PIX_FMT_YUV444P10LE:///< planar YUV 4:4:4: 30bpp: (1 Cr & Cb sample per 1x1 Y samples): little-endian
    case AV_PIX_FMT_YUV422P9BE: ///< planar YUV 4:2:2: 18bpp: (1 Cr & Cb sample per 2x1 Y samples): big-endian
    case AV_PIX_FMT_YUV422P9LE: ///< planar YUV 4:2:2: 18bpp: (1 Cr & Cb sample per 2x1 Y samples): little-endian

    case AV_PIX_FMT_GBRP9BE:   ///< planar GBR 4:4:4 27bpp: big-endian
    case AV_PIX_FMT_GBRP9LE:   ///< planar GBR 4:4:4 27bpp: little-endian
    case AV_PIX_FMT_GBRP10BE:  ///< planar GBR 4:4:4 30bpp: big-endian
    case AV_PIX_FMT_GBRP10LE:  ///< planar GBR 4:4:4 30bpp: little-endian
    case AV_PIX_FMT_GBRP16BE:  ///< planar GBR 4:4:4 48bpp: big-endian
    case AV_PIX_FMT_GBRP16LE:  ///< planar GBR 4:4:4 48bpp: little-endian
    case AV_PIX_FMT_YUVA422P:  ///< planar YUV 4:2:2 24bpp: (1 Cr & Cb sample per 2x1 Y & A samples)
    case AV_PIX_FMT_YUVA420P9BE:  ///< planar YUV 4:2:0 22.5bpp: (1 Cr & Cb sample per 2x2 Y & A samples): big-endian
    case AV_PIX_FMT_YUVA420P9LE:  ///< planar YUV 4:2:0 22.5bpp: (1 Cr & Cb sample per 2x2 Y & A samples): little-endian
    case AV_PIX_FMT_YUVA422P9BE:  ///< planar YUV 4:2:2 27bpp: (1 Cr & Cb sample per 2x1 Y & A samples): big-endian
    case AV_PIX_FMT_YUVA422P9LE:  ///< planar YUV 4:2:2 27bpp: (1 Cr & Cb sample per 2x1 Y & A samples): little-endian
    case AV_PIX_FMT_YUVA444P9BE:  ///< planar YUV 4:4:4 36bpp: (1 Cr & Cb sample per 1x1 Y & A samples): big-endian
    case AV_PIX_FMT_YUVA444P9LE:  ///< planar YUV 4:4:4 36bpp: (1 Cr & Cb sample per 1x1 Y & A samples): little-endian
    case AV_PIX_FMT_YUVA420P10BE: ///< planar YUV 4:2:0 25bpp: (1 Cr & Cb sample per 2x2 Y & A samples: big-endian)
    case AV_PIX_FMT_YUVA420P10LE: ///< planar YUV 4:2:0 25bpp: (1 Cr & Cb sample per 2x2 Y & A samples: little-endian)
    case AV_PIX_FMT_YUVA422P10BE: ///< planar YUV 4:2:2 30bpp: (1 Cr & Cb sample per 2x1 Y & A samples: big-endian)
    case AV_PIX_FMT_YUVA422P10LE: ///< planar YUV 4:2:2 30bpp: (1 Cr & Cb sample per 2x1 Y & A samples: little-endian)
    case AV_PIX_FMT_YUVA444P10BE: ///< planar YUV 4:4:4 40bpp: (1 Cr & Cb sample per 1x1 Y & A samples: big-endian)
    case AV_PIX_FMT_YUVA444P10LE: ///< planar YUV 4:4:4 40bpp: (1 Cr & Cb sample per 1x1 Y & A samples: little-endian)
    case AV_PIX_FMT_YUVA420P16BE: ///< planar YUV 4:2:0 40bpp: (1 Cr & Cb sample per 2x2 Y & A samples: big-endian)
    case AV_PIX_FMT_YUVA420P16LE: ///< planar YUV 4:2:0 40bpp: (1 Cr & Cb sample per 2x2 Y & A samples: little-endian)
    case AV_PIX_FMT_YUVA422P16BE: ///< planar YUV 4:2:2 48bpp: (1 Cr & Cb sample per 2x1 Y & A samples: big-endian)
    case AV_PIX_FMT_YUVA422P16LE: ///< planar YUV 4:2:2 48bpp: (1 Cr & Cb sample per 2x1 Y & A samples: little-endian)
    case AV_PIX_FMT_YUVA444P16BE: ///< planar YUV 4:4:4 64bpp: (1 Cr & Cb sample per 1x1 Y & A samples: big-endian)
    case AV_PIX_FMT_YUVA444P16LE: ///< planar YUV 4:4:4 64bpp, (1 Cr & Cb sample per 1x1 Y & A samples, little-endian)


    case AV_PIX_FMT_YUV420P12BE: ///< planar YUV 4:2:0:18bpp: (1 Cr & Cb sample per 2x2 Y samples): big-endian
    case AV_PIX_FMT_YUV420P12LE: ///< planar YUV 4:2:0:18bpp: (1 Cr & Cb sample per 2x2 Y samples): little-endian
    case AV_PIX_FMT_YUV420P14BE: ///< planar YUV 4:2:0:21bpp: (1 Cr & Cb sample per 2x2 Y samples): big-endian
    case AV_PIX_FMT_YUV420P14LE: ///< planar YUV 4:2:0:21bpp: (1 Cr & Cb sample per 2x2 Y samples): little-endian
    case AV_PIX_FMT_YUV422P12BE: ///< planar YUV 4:2:2:24bpp: (1 Cr & Cb sample per 2x1 Y samples): big-endian
    case AV_PIX_FMT_YUV422P12LE: ///< planar YUV 4:2:2:24bpp: (1 Cr & Cb sample per 2x1 Y samples): little-endian
    case AV_PIX_FMT_YUV422P14BE: ///< planar YUV 4:2:2:28bpp: (1 Cr & Cb sample per 2x1 Y samples): big-endian
    case AV_PIX_FMT_YUV422P14LE: ///< planar YUV 4:2:2:28bpp: (1 Cr & Cb sample per 2x1 Y samples): little-endian
    case AV_PIX_FMT_YUV444P12BE: ///< planar YUV 4:4:4:36bpp: (1 Cr & Cb sample per 1x1 Y samples): big-endian
    case AV_PIX_FMT_YUV444P12LE: ///< planar YUV 4:4:4:36bpp: (1 Cr & Cb sample per 1x1 Y samples): little-endian
    case AV_PIX_FMT_YUV444P14BE: ///< planar YUV 4:4:4:42bpp: (1 Cr & Cb sample per 1x1 Y samples): big-endian
    case AV_PIX_FMT_YUV444P14LE: ///< planar YUV 4:4:4:42bpp: (1 Cr & Cb sample per 1x1 Y samples): little-endian
    case AV_PIX_FMT_GBRP12BE:    ///< planar GBR 4:4:4 36bpp: big-endian
    case AV_PIX_FMT_GBRP12LE:    ///< planar GBR 4:4:4 36bpp: little-endian
    case AV_PIX_FMT_GBRP14BE:    ///< planar GBR 4:4:4 42bpp: big-endian
    case AV_PIX_FMT_GBRP14LE:    ///< planar GBR 4:4:4 42bpp: little-endian
    case AV_PIX_FMT_YUVJ411P:    ///< planar YUV 4:1:1: 12bpp: (1 Cr & Cb sample per 4x1 Y samples) full scale (JPEG): deprecated in favor of AV_PIX_FMT_YUV411P and setting color_range


    case AV_PIX_FMT_YUV440P10LE: ///< planar YUV 4:4:0:20bpp: (1 Cr & Cb sample per 1x2 Y samples): little-endian
    case AV_PIX_FMT_YUV440P10BE: ///< planar YUV 4:4:0:20bpp: (1 Cr & Cb sample per 1x2 Y samples): big-endian
    case AV_PIX_FMT_YUV440P12LE: ///< planar YUV 4:4:0:24bpp: (1 Cr & Cb sample per 1x2 Y samples): little-endian
    case AV_PIX_FMT_YUV440P12BE: ///< planar YUV 4:4:0:24bpp: (1 Cr & Cb sample per 1x2 Y samples): big-endian


    case AV_PIX_FMT_GBRAP12BE:  ///< planar GBR 4:4:4:4 48bpp: big-endian
    case AV_PIX_FMT_GBRAP12LE:  ///< planar GBR 4:4:4:4 48bpp: little-endian

    case AV_PIX_FMT_GBRAP10BE:  ///< planar GBR 4:4:4:4 40bpp: big-endian
    case AV_PIX_FMT_GBRAP10LE:  ///< planar GBR 4:4:4:4 40bpp: little-endian


    case AV_PIX_FMT_GBRPF32BE:  ///< IEEE-754 single precision planar GBR 4:4:4:     96bpp: big-endian
    case AV_PIX_FMT_GBRPF32LE:  ///< IEEE-754 single precision planar GBR 4:4:4:     96bpp: little-endian
    case AV_PIX_FMT_GBRAPF32BE: ///< IEEE-754 single precision planar GBRA 4:4:4:4: 128bpp: big-endian
    case AV_PIX_FMT_GBRAPF32LE: ///< IEEE-754 single precision planar GBRA 4:4:4:4: 128bpp: little-endian

    case AV_PIX_FMT_YUVA422P12BE: ///< planar YUV 4:2:2:24bpp: (1 Cr & Cb sample per 2x1 Y samples): 12b alpha: big-endian
    case AV_PIX_FMT_YUVA422P12LE: ///< planar YUV 4:2:2:24bpp: (1 Cr & Cb sample per 2x1 Y samples): 12b alpha: little-endian
    case AV_PIX_FMT_YUVA444P12BE: ///< planar YUV 4:4:4:36bpp: (1 Cr & Cb sample per 1x1 Y samples): 12b alpha: big-endian
    case AV_PIX_FMT_YUVA444P12LE: ///< planar YUV 4:4:4:36bpp: (1 Cr & Cb sample per 1x1 Y samples): 12b alpha: little-endian
    {
      qDebug() << "TODO unhandled video format";
      free(p);
      break;
    }

    case AV_PIX_FMT_MONOWHITE: ///<        Y        :  1bpp: 0 is white: 1 is black: in each byte pixels are ordered from the msb to the lsb
    case AV_PIX_FMT_MONOBLACK: ///<        Y        :  1bpp: 0 is black: 1 is white: in each byte pixels are ordered from the msb to the lsb
      frame->data[0] = p;
      frame->linesize[0] = frame->width / CHAR_BIT;
      break;

    case AV_PIX_FMT_BGR4:      ///< packed RGB 1:2:1 bitstream:  4bpp: (msb)1B 2G 1R(lsb): a byte contains two pixels: the first pixel in the byte is the one composed by the 4 msb bits
    case AV_PIX_FMT_RGB4:      ///< packed RGB 1:2:1 bitstream:  4bpp: (msb)1R 2G 1B(lsb): a byte contains two pixels: the first pixel in the byte is the one composed by the 4 msb bits
      frame->data[0] = p;
      frame->linesize[0] = frame->width * 4. / 8.;
      break;

    case AV_PIX_FMT_GRAY8:     ///<        Y        :  8bpp
    case AV_PIX_FMT_BGR8:      ///< packed RGB 3:3:2:  8bpp: (msb)2B 3G 3R(lsb)
    case AV_PIX_FMT_RGB8:      ///< packed RGB 3:3:2:  8bpp: (msb)2R 3G 3B(lsb)
    case AV_PIX_FMT_BGR4_BYTE: ///< packed RGB 1:2:1:  8bpp: (msb)1B 2G 1R(lsb)
    case AV_PIX_FMT_RGB4_BYTE: ///< packed RGB 1:2:1:  8bpp: (msb)1R 2G 1B(lsb)
      frame->data[0] = p;
      frame->linesize[0] = frame->width * 8. / 8.;
      break;

    case AV_PIX_FMT_GRAY9BE:   ///<        Y        : 9bpp: big-endian
    case AV_PIX_FMT_GRAY9LE:   ///<        Y        : 9bpp: little-endian
      frame->data[0] = p;
      frame->linesize[0] = frame->width * 9. / 8.; // ?? wtf
      break;

    case AV_PIX_FMT_P010LE: ///< like NV12: with 10bpp per component: data in the high bits: zeros in the low bits: little-endian
    case AV_PIX_FMT_P010BE: ///< like NV12: with 10bpp per component: data in the high bits: zeros in the low bits: big-endian
    case AV_PIX_FMT_GRAY10BE:   ///<        Y        : 10bpp: big-endian
    case AV_PIX_FMT_GRAY10LE:   ///<        Y        : 10bpp: little-endian
      frame->data[0] = p;
      frame->linesize[0] = frame->width * 10. / 8.; // ??
      break;

    case AV_PIX_FMT_GRAY12BE:   ///<        Y        : 12bpp: big-endian
    case AV_PIX_FMT_GRAY12LE:   ///<        Y        : 12bpp: little-endian
    case AV_PIX_FMT_UYYVYY411: ///< packed YUV 4:1:1: 12bpp: Cb Y0 Y1 Cr Y2 Y3
      frame->data[0] = p;
      frame->linesize[0] = frame->width * 12. / 8.; // ??
      break;

    case AV_PIX_FMT_GRAY14BE:   ///<        Y        : 14bpp: big-endian
    case AV_PIX_FMT_GRAY14LE:   ///<        Y        : 14bpp: little-endian
      frame->data[0] = p;
      frame->linesize[0] = frame->width * 14. / 8.; // ??
      break;

    case AV_PIX_FMT_NV16:         ///< interleaved chroma YUV 4:2:2: 16bpp: (1 Cr & Cb sample per 2x1 Y samples)
    case AV_PIX_FMT_YUYV422:   ///< packed YUV 4:2:2: 16bpp: Y0 Cb Y1 Cr
    case AV_PIX_FMT_UYVY422:   ///< packed YUV 4:2:2: 16bpp: Cb Y0 Cr Y1
    case AV_PIX_FMT_YVYU422:   ///< packed YUV 4:2:2: 16bpp: Y0 Cr Y1 Cb
    case AV_PIX_FMT_GRAY16BE:  ///<        Y        : 16bpp: big-endian
    case AV_PIX_FMT_GRAY16LE:  ///<        Y        : 16bpp: little-endian
    case AV_PIX_FMT_YA8:       ///< 8 bits gray: 8 bits alpha

    case AV_PIX_FMT_RGB565BE:  ///< packed RGB 5:6:5: 16bpp: (msb)   5R 6G 5B(lsb): big-endian
    case AV_PIX_FMT_RGB565LE:  ///< packed RGB 5:6:5: 16bpp: (msb)   5R 6G 5B(lsb): little-endian
    case AV_PIX_FMT_RGB555BE:  ///< packed RGB 5:5:5: 16bpp: (msb)1X 5R 5G 5B(lsb): big-endian   : X=unused/undefined
    case AV_PIX_FMT_RGB555LE:  ///< packed RGB 5:5:5: 16bpp: (msb)1X 5R 5G 5B(lsb): little-endian: X=unused/undefined

    case AV_PIX_FMT_BGR565BE:  ///< packed BGR 5:6:5: 16bpp: (msb)   5B 6G 5R(lsb): big-endian
    case AV_PIX_FMT_BGR565LE:  ///< packed BGR 5:6:5: 16bpp: (msb)   5B 6G 5R(lsb): little-endian
    case AV_PIX_FMT_BGR555BE:  ///< packed BGR 5:5:5: 16bpp: (msb)1X 5B 5G 5R(lsb): big-endian   : X=unused/undefined
    case AV_PIX_FMT_BGR555LE:  ///< packed BGR 5:5:5: 16bpp: (msb)1X 5B 5G 5R(lsb): little-endian: X=unused/undefined

    case AV_PIX_FMT_RGB444LE:  ///< packed RGB 4:4:4: 16bpp: (msb)4X 4R 4G 4B(lsb): little-endian: X=unused/undefined
    case AV_PIX_FMT_RGB444BE:  ///< packed RGB 4:4:4: 16bpp: (msb)4X 4R 4G 4B(lsb): big-endian:    X=unused/undefined
    case AV_PIX_FMT_BGR444LE:  ///< packed BGR 4:4:4: 16bpp: (msb)4X 4B 4G 4R(lsb): little-endian: X=unused/undefined
    case AV_PIX_FMT_BGR444BE:  ///< packed BGR 4:4:4: 16bpp: (msb)4X 4B 4G 4R(lsb): big-endian:    X=unused/undefined
      frame->data[0] = p;
      frame->linesize[0] = frame->width * 16. / 8.;
      break;

    case AV_PIX_FMT_NV20LE:       ///< interleaved chroma YUV 4:2:2: 20bpp: (1 Cr & Cb sample per 2x1 Y samples): little-endian
    case AV_PIX_FMT_NV20BE:       ///< interleaved chroma YUV 4:2:2: 20bpp: (1 Cr & Cb sample per 2x1 Y samples): big-endian
    case AV_PIX_FMT_Y210BE:    ///< packed YUV 4:2:2 like YUYV422: 20bpp: data in the high bits: big-endian
    case AV_PIX_FMT_Y210LE:    ///< packed YUV 4:2:2 like YUYV422: 20bpp: data in the high bits: little-endian
      frame->data[0] = p;
      frame->linesize[0] = frame->width * 20. / 8.;
      break;

    case AV_PIX_FMT_RGB24:     ///< packed RGB 8:8:8: 24bpp: RGBRGB...
    case AV_PIX_FMT_BGR24:     ///< packed RGB 8:8:8: 24bpp: BGRBGR...
      frame->data[0] = p;
      frame->linesize[0] = frame->width * 24. / 8.;
      break;

    // needs a too recent ffmpeg?
    // case AV_PIX_FMT_X2RGB10LE: ///< packed RGB 10:10:10: 30bpp: (msb)2X 10R 10G 10B(lsb): little-endian: X=unused/undefined
    // case AV_PIX_FMT_X2RGB10BE: ///< packed RGB 10:10:10: 30bpp: (msb)2X 10R 10G 10B(lsb): big-endian: X=unused/undefined
    //   frame->data[0] = p;
    //   frame->linesize[0] = frame->width * 30. / 8.;
    //   break;

    case AV_PIX_FMT_ARGB:      ///< packed ARGB 8:8:8:8: 32bpp: ARGBARGB...
    case AV_PIX_FMT_RGBA:      ///< packed RGBA 8:8:8:8: 32bpp: RGBARGBA...
    case AV_PIX_FMT_ABGR:      ///< packed ABGR 8:8:8:8: 32bpp: ABGRABGR...
    case AV_PIX_FMT_BGRA:      ///< packed BGRA 8:8:8:8: 32bpp: BGRABGRA...
    case AV_PIX_FMT_0RGB:        ///< packed RGB 8:8:8: 32bpp: XRGBXRGB...   X=unused/undefined
    case AV_PIX_FMT_RGB0:        ///< packed RGB 8:8:8: 32bpp: RGBXRGBX...   X=unused/undefined
    case AV_PIX_FMT_0BGR:        ///< packed BGR 8:8:8: 32bpp: XBGRXBGR...   X=unused/undefined
    case AV_PIX_FMT_BGR0:        ///< packed BGR 8:8:8: 32bpp: BGRXBGRX...   X=unused/undefined
    case AV_PIX_FMT_GRAYF32BE:  ///< IEEE-754 single precision Y: 32bpp: big-endian
    case AV_PIX_FMT_GRAYF32LE:  ///< IEEE-754 single precision Y: 32bpp: little-endian
    case AV_PIX_FMT_YA16BE:       ///< 16 bits gray: 16 bits alpha (big-endian)
    case AV_PIX_FMT_YA16LE:       ///< 16 bits gray: 16 bits alpha (little-endian)
      frame->data[0] = p;
      frame->linesize[0] = frame->width * 32. / 8.;
      break;


    case AV_PIX_FMT_XYZ12LE:      ///< packed XYZ 4:4:4: 36 bpp: (msb) 12X: 12Y: 12Z (lsb): the 2-byte value for each X/Y/Z is stored as little-endian: the 4 lower bits are set to 0
    case AV_PIX_FMT_XYZ12BE:      ///< packed XYZ 4:4:4: 36 bpp: (msb) 12X: 12Y: 12Z (lsb): the 2-byte value for each X/Y/Z is stored as big-endian: the 4 lower bits are set to 0
      frame->data[0] = p;
      frame->linesize[0] = frame->width * 36. / 8.;
      break;

    case AV_PIX_FMT_RGB48BE:   ///< packed RGB 16:16:16: 48bpp: 16R: 16G: 16B: the 2-byte value for each R/G/B component is stored as big-endian
    case AV_PIX_FMT_RGB48LE:   ///< packed RGB 16:16:16: 48bpp: 16R: 16G: 16B: the 2-byte value for each R/G/B component is stored as little-endian

    case AV_PIX_FMT_BGR48BE:   ///< packed RGB 16:16:16: 48bpp: 16B: 16G: 16R: the 2-byte value for each R/G/B component is stored as big-endian
    case AV_PIX_FMT_BGR48LE:   ///< packed RGB 16:16:16: 48bpp: 16B: 16G: 16R: the 2-byte value for each R/G/B component is stored as little-endian
      frame->data[0] = p;
      frame->linesize[0] = frame->width * 48. / 8.;
      break;

    case AV_PIX_FMT_RGBA64BE:     ///< packed RGBA 16:16:16:16: 64bpp: 16R: 16G: 16B: 16A: the 2-byte value for each R/G/B/A component is stored as big-endian
    case AV_PIX_FMT_RGBA64LE:     ///< packed RGBA 16:16:16:16: 64bpp: 16R: 16G: 16B: 16A: the 2-byte value for each R/G/B/A component is stored as little-endian
    case AV_PIX_FMT_BGRA64BE:     ///< packed RGBA 16:16:16:16: 64bpp: 16B: 16G: 16R: 16A: the 2-byte value for each R/G/B/A component is stored as big-endian
    case AV_PIX_FMT_BGRA64LE:     ///< packed RGBA 16:16:16:16: 64bpp: 16B: 16G: 16R: 16A: the 2-byte value for each R/G/B/A component is stored as little-endian
    case AV_PIX_FMT_AYUV64LE:    ///< packed AYUV 4:4:4:64bpp (1 Cr & Cb sample per 1x1 Y & A samples): little-endian
    case AV_PIX_FMT_AYUV64BE:    ///< packed AYUV 4:4:4:64bpp (1 Cr & Cb sample per 1x1 Y & A samples): big-endian
      frame->data[0] = p;
      frame->linesize[0] = frame->width * 64. / 8.;
      break;



    case AV_PIX_FMT_BAYER_BGGR8:    ///< bayer: BGBG..(odd line): GRGR..(even line): 8-bit samples
    case AV_PIX_FMT_BAYER_RGGB8:    ///< bayer: RGRG..(odd line): GBGB..(even line): 8-bit samples
    case AV_PIX_FMT_BAYER_GBRG8:    ///< bayer: GBGB..(odd line): RGRG..(even line): 8-bit samples
    case AV_PIX_FMT_BAYER_GRBG8:    ///< bayer: GRGR..(odd line): BGBG..(even line): 8-bit samples
    case AV_PIX_FMT_BAYER_BGGR16LE: ///< bayer: BGBG..(odd line): GRGR..(even line): 16-bit samples: little-endian
    case AV_PIX_FMT_BAYER_BGGR16BE: ///< bayer: BGBG..(odd line): GRGR..(even line): 16-bit samples: big-endian
    case AV_PIX_FMT_BAYER_RGGB16LE: ///< bayer: RGRG..(odd line): GBGB..(even line): 16-bit samples: little-endian
    case AV_PIX_FMT_BAYER_RGGB16BE: ///< bayer: RGRG..(odd line): GBGB..(even line): 16-bit samples: big-endian
    case AV_PIX_FMT_BAYER_GBRG16LE: ///< bayer: GBGB..(odd line): RGRG..(even line): 16-bit samples: little-endian
    case AV_PIX_FMT_BAYER_GBRG16BE: ///< bayer: GBGB..(odd line): RGRG..(even line): 16-bit samples: big-endian
    case AV_PIX_FMT_BAYER_GRBG16LE: ///< bayer: GRGR..(odd line): BGBG..(even line): 16-bit samples: little-endian
    case AV_PIX_FMT_BAYER_GRBG16BE: ///< bayer: GRGR..(odd line): BGBG..(even line): 16-bit samples: big-endian
    case AV_PIX_FMT_PAL8:      ///< 8 bits with case AV_PIX_FMT_RGB32 palette
    default:
    {
      qDebug() << "TODO unhandled video format";
      free(p);
      break;
    }
  }
}

}
#endif

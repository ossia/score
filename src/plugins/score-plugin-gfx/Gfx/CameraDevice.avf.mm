#include "CameraDevice.hpp"

#include "CameraSettings.hpp"

#include <Gfx/CameraDeviceEnumerator.hpp>

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#include <libavutil/pixfmt.h>

#include <functional>
#include <iostream>
#include <memory>

namespace Gfx
{
struct avf_to_ffmpeg
{
    constexpr avf_to_ffmpeg() = default;
    constexpr avf_to_ffmpeg(AVPixelFormat fmt, AVColorRange range)
        : fmt{fmt}
        , range{range}
    {}
    AVPixelFormat fmt{AV_PIX_FMT_NONE};
    AVColorRange range{AVCOL_RANGE_UNSPECIFIED};
};

static constexpr avf_to_ffmpeg avf_pixelformat_to_ffmpeg(uint32_t fourcc)
{
  switch(fourcc)
  {
    case kCMPixelFormat_32ARGB:
        return {AV_PIX_FMT_ARGB, AVCOL_RANGE_JPEG};
    case kCMPixelFormat_32BGRA:
        return {AV_PIX_FMT_BGRA, AVCOL_RANGE_JPEG};
    case kCMPixelFormat_24RGB:
        return {AV_PIX_FMT_RGB24, AVCOL_RANGE_JPEG};
    case kCMPixelFormat_16BE555:
        return {AV_PIX_FMT_RGB555BE, AVCOL_RANGE_JPEG};
    case kCMPixelFormat_16BE565:
        return {AV_PIX_FMT_RGB565BE, AVCOL_RANGE_JPEG};
    case kCMPixelFormat_16LE555:
        return {AV_PIX_FMT_RGB555LE, AVCOL_RANGE_JPEG};
    case kCMPixelFormat_16LE565:
        return {AV_PIX_FMT_RGB565LE, AVCOL_RANGE_JPEG};
    case kCMPixelFormat_16LE5551:
        return {};
    case 'yuv2':
    case 'YUV2':
    case 'uyvy':
    case 'UYVY':
    case kCMPixelFormat_422YpCbCr8:
        return {AV_PIX_FMT_UYVY422, AVCOL_RANGE_MPEG};
    case kCMPixelFormat_422YpCbCr8_yuvs:
        return {AV_PIX_FMT_YUYV422, AVCOL_RANGE_MPEG};
    case kCMPixelFormat_444YpCbCr8: // WARNING ! this is planar, macOS's is packed. check that it's ok and ffmpeg assumes the conversion
        return {AV_PIX_FMT_YUV444P, AVCOL_RANGE_MPEG};
    case kCMPixelFormat_4444YpCbCrA8:
        return {AV_PIX_FMT_YUVA444P, AVCOL_RANGE_MPEG};
    case kCMPixelFormat_422YpCbCr16:
        return {AV_PIX_FMT_YUV422P16LE, AVCOL_RANGE_MPEG};
    case kCMPixelFormat_422YpCbCr10:
        return {AV_PIX_FMT_YUV422P10LE, AVCOL_RANGE_MPEG};
    case kCMPixelFormat_444YpCbCr10:
        return {AV_PIX_FMT_YUV444P10LE, AVCOL_RANGE_MPEG};
    case kCMPixelFormat_8IndexedGray_WhiteIsZero:
        return {};

    case kCVPixelFormatType_1Monochrome:              /* 1 bit indexed */
    case kCVPixelFormatType_2Indexed:                 /* 2 bit indexed */
    case kCVPixelFormatType_4Indexed:                 /* 4 bit indexed */
    case kCVPixelFormatType_8Indexed:                 /* 8 bit indexed */
    case kCVPixelFormatType_1IndexedGray_WhiteIsZero: /* 1 bit indexed gray, white is zero */
    case kCVPixelFormatType_2IndexedGray_WhiteIsZero: /* 2 bit indexed gray, white is zero */
    case kCVPixelFormatType_4IndexedGray_WhiteIsZero: /* 4 bit indexed gray, white is zero */
    // case kCVPixelFormatType_8IndexedGray_WhiteIsZero: /* 8 bit indexed gray, white is zero */
    // case kCVPixelFormatType_16BE555:                  /* 16 bit BE RGB 555 */
    // case kCVPixelFormatType_16LE555:                  /* 16 bit LE RGB 555 */
    // case kCVPixelFormatType_16LE5551:                 /* 16 bit LE RGB 5551 */
    // case kCVPixelFormatType_16BE565:                  /* 16 bit BE RGB 565 */
    // case kCVPixelFormatType_16LE565:                  /* 16 bit LE RGB 565 */
    // case kCVPixelFormatType_24RGB:                    /* 24 bit RGB */
    case kCVPixelFormatType_24BGR: /* 24 bit BGR */
        return {AV_PIX_FMT_BGR24, AVCOL_RANGE_JPEG};
    // case kCVPixelFormatType_32ARGB:                   /* 32 bit ARGB */
    // case kCVPixelFormatType_32BGRA:                   /* 32 bit BGRA */
    case kCVPixelFormatType_32ABGR: /* 32 bit ABGR */
        return {AV_PIX_FMT_ABGR, AVCOL_RANGE_JPEG};
    case kCVPixelFormatType_32RGBA: /* 32 bit RGBA */
        return {AV_PIX_FMT_RGBA, AVCOL_RANGE_JPEG};
    case kCVPixelFormatType_64ARGB: /* 64 bit ARGB, 16-bit big-endian samples */
        return {};
    case kCVPixelFormatType_64RGBALE: /* 64 bit RGBA, 16-bit little-endian full-range (0-65535) samples */
        return {};
    case kCVPixelFormatType_48RGB: /* 48 bit RGB, 16-bit big-endian samples */
        return {};
    case kCVPixelFormatType_32AlphaGray: /* 32 bit AlphaGray, 16-bit big-endian samples, black is zero */
        return {};
    case kCVPixelFormatType_16Gray: /* 16 bit Grayscale, 16-bit big-endian samples, black is zero */
        return {AV_PIX_FMT_GRAY16BE, AVCOL_RANGE_JPEG};
    case kCVPixelFormatType_30RGB: /* 30 bit RGB, 10-bit big-endian samples, 2 unused padding bits (at least significant end). */
        return {};
#if 0
    case kCVPixelFormatType_30RGB_r210: /* 30 bit RGB, 10-bit big-endian samples, 2 unused padding bits (at most significant end), video-range (64-940). */
        return {};
#endif
    // case kCVPixelFormatType_422YpCbCr8: /* Component Y'CbCr 8-bit 4:2:2, ordered Cb Y'0 Cr Y'1 */
    // case kCVPixelFormatType_4444YpCbCrA8:  /* Component Y'CbCrA 8-bit 4:4:4:4, ordered Cb Y' Cr A */
    case kCVPixelFormatType_4444YpCbCrA8R: /* Component Y'CbCrA 8-bit 4:4:4:4, rendering format. full range alpha, zero biased YUV, ordered A Y' Cb Cr */
        return {};                         //AV_PIX_FMT_YUVA444P; ?
    case kCVPixelFormatType_4444AYpCbCr8: /* Component Y'CbCrA 8-bit 4:4:4:4, ordered A Y' Cb Cr, full range alpha, video range Y'CbCr. */
        return {};
    case kCVPixelFormatType_4444AYpCbCr16: /* Component Y'CbCrA 16-bit 4:4:4:4, ordered A Y' Cb Cr, full range alpha, video range Y'CbCr, 16-bit little-endian samples. */
        return {AV_PIX_FMT_AYUV64LE, AVCOL_RANGE_MPEG};
    case kCVPixelFormatType_4444AYpCbCrFloat: /* Component AY'CbCr single precision floating-point 4:4:4:4 */
        return {};
        // case kCVPixelFormatType_444YpCbCr8: /* Component Y'CbCr 8-bit 4:4:4, ordered Cr Y' Cb, video range Y'CbCr */
        // case kCVPixelFormatType_422YpCbCr16:      /* Component Y'CbCr 10,12,14,16-bit 4:2:2 */
        // case kCVPixelFormatType_422YpCbCr10:      /* Component Y'CbCr 10-bit 4:2:2 */
        // case kCVPixelFormatType_444YpCbCr10:      /* Component Y'CbCr 10-bit 4:4:4 */
        return {};                            // TODO
    case kCVPixelFormatType_420YpCbCr8Planar: /* Planar Component Y'CbCr 8-bit 4:2:0.  baseAddr points to a big-endian CVPlanarPixelBufferInfo_YCbCrPlanar struct */
        return {AV_PIX_FMT_YUV420P, AVCOL_RANGE_MPEG};
    case kCVPixelFormatType_420YpCbCr8PlanarFullRange: /* Planar Component Y'CbCr 8-bit 4:2:0, full range.  baseAddr points to a big-endian CVPlanarPixelBufferInfo_YCbCrPlanar struct */
        return {AV_PIX_FMT_YUV420P, AVCOL_RANGE_JPEG}; // FIXME fullrange / video range
    case kCVPixelFormatType_422YpCbCr_4A_8BiPlanar: /* First plane: Video-range Component Y'CbCr 8-bit 4:2:2, ordered Cb Y'0 Cr Y'1; second plane: alpha 8-bit 0-255 */
        return {};
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        return {AV_PIX_FMT_NV12, AVCOL_RANGE_MPEG};
        /* Bi-Planar Component Y'CbCr 8-bit 4:2:0, video-range (luma=[16,235] chroma=[16,240]).  baseAddr points to a big-endian CVPlanarPixelBufferInfo_YCbCrBiPlanar struct */
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange: /* Bi-Planar Component Y'CbCr 8-bit 4:2:0, full-range (luma=[0,255] chroma=[1,255]).  baseAddr points to a big-endian CVPlanarPixelBufferInfo_YCbCrBiPlanar struct */
        return {AV_PIX_FMT_NV12, AVCOL_RANGE_JPEG};
    case kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange: /* Bi-Planar Component Y'CbCr 8-bit 4:2:2, video-range (luma=[16,235] chroma=[16,240]).  baseAddr points to a big-endian CVPlanarPixelBufferInfo_YCbCrBiPlanar struct */
        return {AV_PIX_FMT_NV16, AVCOL_RANGE_MPEG};
    case kCVPixelFormatType_422YpCbCr8BiPlanarFullRange: /* Bi-Planar Component Y'CbCr 8-bit 4:2:2, full-range (luma=[0,255] chroma=[1,255]).  baseAddr points to a big-endian CVPlanarPixelBufferInfo_YCbCrBiPlanar struct */
        return {AV_PIX_FMT_NV16, AVCOL_RANGE_JPEG};
    case kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange: /* Bi-Planar Component Y'CbCr 8-bit 4:4:4, video-range (luma=[16,235] chroma=[16,240]).  baseAddr points to a big-endian CVPlanarPixelBufferInfo_YCbCrBiPlanar struct */
        return {AV_PIX_FMT_NV24, AVCOL_RANGE_MPEG};
    case kCVPixelFormatType_444YpCbCr8BiPlanarFullRange: /* Bi-Planar Component Y'CbCr 8-bit 4:4:4, full-range (luma=[0,255] chroma=[1,255]).  baseAddr points to a big-endian CVPlanarPixelBufferInfo_YCbCrBiPlanar struct */
        return {AV_PIX_FMT_NV24, AVCOL_RANGE_JPEG};
        // case kCVPixelFormatType_422YpCbCr8_yuvs: /* Component Y'CbCr 8-bit 4:2:2, ordered Y'0 Cb Y'1 Cr */
    case kCVPixelFormatType_422YpCbCr8FullRange: /* Component Y'CbCr 8-bit 4:2:2, full range, ordered Y'0 Cb Y'1 Cr */
        return {AV_PIX_FMT_YUYV422, AVCOL_RANGE_JPEG};
    case kCVPixelFormatType_OneComponent8: /* 8 bit one component, black is zero */
    case kCVPixelFormatType_TwoComponent8: /* 8 bit two component, black is zero */
    case kCVPixelFormatType_30RGBLEPackedWideGamut: /* little-endian RGB101010, 2 MSB are ignored, wide-gamut (384-895) */
    case kCVPixelFormatType_ARGB2101010LEPacked: /* little-endian ARGB2101010 full-range ARGB */
    case kCVPixelFormatType_40ARGBLEWideGamut: /* little-endian ARGB10101010, each 10 bits in the MSBs of 16bits, wide-gamut (384-895, including alpha) */
    case kCVPixelFormatType_40ARGBLEWideGamutPremultiplied: /* little-endian ARGB10101010, each 10 bits in the MSBs of 16bits, wide-gamut (384-895, including alpha). Alpha premultiplied */
    case kCVPixelFormatType_OneComponent10: /* 10 bit little-endian one component, stored as 10 MSBs of 16 bits, black is zero */
    case kCVPixelFormatType_OneComponent12: /* 12 bit little-endian one component, stored as 12 MSBs of 16 bits, black is zero */
    case kCVPixelFormatType_OneComponent16: /* 16 bit little-endian one component, black is zero */
    case kCVPixelFormatType_TwoComponent16: /* 16 bit little-endian two component, black is zero */
    case kCVPixelFormatType_OneComponent16Half: /* 16 bit one component IEEE half-precision float, 16-bit little-endian samples */
    case kCVPixelFormatType_OneComponent32Float: /* 32 bit one component IEEE float, 32-bit little-endian samples */
    case kCVPixelFormatType_TwoComponent16Half: /* 16 bit two component IEEE half-precision float, 16-bit little-endian samples */
    case kCVPixelFormatType_TwoComponent32Float: /* 32 bit two component IEEE float, 32-bit little-endian samples */
    case kCVPixelFormatType_64RGBAHalf: /* 64 bit RGBA IEEE half-precision float, 16-bit little-endian samples */
    case kCVPixelFormatType_128RGBAFloat: /* 128 bit RGBA IEEE float, 32-bit little-endian samples */
    case kCVPixelFormatType_14Bayer_GRBG: /* Bayer 14-bit Little-Endian, packed in 16-bits, ordered G R G R... alternating with B G B G... */
    case kCVPixelFormatType_14Bayer_RGGB: /* Bayer 14-bit Little-Endian, packed in 16-bits, ordered R G R G... alternating with G B G B... */
    case kCVPixelFormatType_14Bayer_BGGR: /* Bayer 14-bit Little-Endian, packed in 16-bits, ordered B G B G... alternating with G R G R... */
    case kCVPixelFormatType_14Bayer_GBRG: /* Bayer 14-bit Little-Endian, packed in 16-bits, ordered G B G B... alternating with R G R G... */
    case kCVPixelFormatType_DisparityFloat16: /* IEEE754-2008 binary16 (half float), describing the normalized shift when comparing two images. Units are 1/meters: ( pixelShift / (pixelFocalLength * baselineInMeters) ) */
    case kCVPixelFormatType_DisparityFloat32: /* IEEE754-2008 binary32 float, describing the normalized shift when comparing two images. Units are 1/meters: ( pixelShift / (pixelFocalLength * baselineInMeters) ) */
    case kCVPixelFormatType_DepthFloat16: /* IEEE754-2008 binary16 (half float), describing the depth (distance to an object) in meters */
    case kCVPixelFormatType_DepthFloat32: /* IEEE754-2008 binary32 float, describing the depth (distance to an object) in meters */
        return {};
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange: /* 2 plane YCbCr10 4:2:0, each 10 bits in the MSBs of 16bits, video-range (luma=[64,940] chroma=[64,960]) */
        return {AV_PIX_FMT_P010, AVCOL_RANGE_MPEG};
    case kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange: /* 2 plane YCbCr10 4:2:2, each 10 bits in the MSBs of 16bits, video-range (luma=[64,940] chroma=[64,960]) */
        return {AV_PIX_FMT_P210, AVCOL_RANGE_MPEG};
    case kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange: /* 2 plane YCbCr10 4:4:4, each 10 bits in the MSBs of 16bits, video-range (luma=[64,940] chroma=[64,960]) */
        return {AV_PIX_FMT_P410, AVCOL_RANGE_MPEG};
    case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange: /* 2 plane YCbCr10 4:2:0, each 10 bits in the MSBs of 16bits, full-range (Y range 0-1023) */
        return {AV_PIX_FMT_P010, AVCOL_RANGE_JPEG};
    case kCVPixelFormatType_422YpCbCr10BiPlanarFullRange: /* 2 plane YCbCr10 4:2:2, each 10 bits in the MSBs of 16bits, full-range (Y range 0-1023) */
        return {AV_PIX_FMT_P210, AVCOL_RANGE_JPEG};
    case kCVPixelFormatType_444YpCbCr10BiPlanarFullRange: /* 2 plane YCbCr10 4:4:4, each 10 bits in the MSBs of 16bits, full-range (Y range 0-1023) */
        return {AV_PIX_FMT_P410, AVCOL_RANGE_JPEG};

    case kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar: /* first and second planes as per 420YpCbCr8BiPlanarVideoRange (420v), alpha 8 bits in third plane full-range.  No CVPlanarPixelBufferInfo struct. */
    case kCVPixelFormatType_16VersatileBayer: /* Single plane Bayer 16-bit little-endian sensor element ("sensel") samples from full-size decoding of ProRes RAW images; Bayer pattern (sensel ordering) and other raw conversion information is described via buffer attachments */
    case kCVPixelFormatType_64RGBA_DownscaledProResRAW: /* Single plane 64-bit RGBA (16-bit little-endian samples) from downscaled decoding of ProRes RAW images; components--which may not be co-sited with one another--are sensel values and require raw conversion, information for which is described via buffer attachments */
    case kCVPixelFormatType_422YpCbCr16BiPlanarVideoRange: /* 2 plane YCbCr16 4:2:2, video-range (luma=[4096,60160] chroma=[4096,61440]) */
    case kCVPixelFormatType_444YpCbCr16BiPlanarVideoRange: /* 2 plane YCbCr16 4:4:4, video-range (luma=[4096,60160] chroma=[4096,61440]) */
        return {AV_PIX_FMT_P416, AVCOL_RANGE_MPEG};
    case kCVPixelFormatType_444YpCbCr16VideoRange_16A_TriPlanar: /* 3 plane video-range YCbCr16 4:4:4 with 16-bit full-range alpha (luma=[4096,60160] chroma=[4096,61440] alpha=[0,65535]).  No CVPlanarPixelBufferInfo struct. */

    default:
        return {};
  }
}

struct AVFCameraEnumerator : public Device::DeviceEnumerator
{
  explicit AVFCameraEnumerator(
      std::shared_ptr<CameraDeviceEnumerator> parent, AVCaptureDevice* dev)
      : parent{parent}
      , device{dev}
  {
  }
  std::shared_ptr<CameraDeviceEnumerator> parent;
  AVCaptureDevice* device{};
  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> func)
      const override
  {
    const char* name = [[device localizedName] UTF8String];
    for(id format in [device valueForKey:@"formats"])
    {
      CMFormatDescriptionRef formatDescription{};
      CMVideoDimensions dimensions{};

      formatDescription = (__bridge CMFormatDescriptionRef)
          [format performSelector:@selector(formatDescription)];
      dimensions = CMVideoFormatDescriptionGetDimensions(formatDescription);
      auto fourcc = CMFormatDescriptionGetMediaSubType(formatDescription);
      std::string fcc((const char *) &fourcc, 4);
      std::reverse(fcc.begin(), fcc.end());
      for(id range in [format valueForKey:@"videoSupportedFrameRateRanges"])
      {
        double fps{};

        [[range valueForKey:@"maxFrameRate"] getValue:&fps];

        const auto mode = QString("%1x%2@%3 %4")
                              .arg(dimensions.width)
                              .arg(dimensions.height)
                              .arg(fps)
                              .arg(std::string(fcc).c_str());

        Device::DeviceSettings s;
        CameraSettings set;
        set.input = "avfoundation";
        set.device = name;
        set.size = {dimensions.width, dimensions.height};
        set.fps = fps;
        auto [pixfmt, colrange] = avf_pixelformat_to_ffmpeg(fourcc);
        set.pixelformat = pixfmt;
        set.colorRange = colrange;
        s.name = name;
        s.protocol = CameraProtocolFactory::static_concreteKey();
        s.deviceSpecificSettings = QVariant::fromValue(set);
        func(mode, s);
      }
    }
  }
};

struct AVFCameraDeviceEnumerator : public CameraDeviceEnumerator
{
  void registerAllEnumerators(Device::DeviceEnumerators& enums) override
  {
    auto self = shared_from_this();
    SCORE_ASSERT(self);

    {
      NSArray* devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
      for(AVCaptureDevice* device in devices)
      {
        const char* name = [[device localizedName] UTF8String];
        enums.push_back({name, new AVFCameraEnumerator{self, device}});
      }
    }

    {
      NSArray* devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeMuxed];
      for(AVCaptureDevice* device in devices)
      {
        const char* name = [[device localizedName] UTF8String];
        enums.push_back({name, new AVFCameraEnumerator{self, device}});
      }
    }
  }
};

void enumerateCameraDevices(std::function<void(CameraSettings, QString)> func)
{
  Device::DeviceEnumerators enums;
  AVFCameraDeviceEnumerator root;
  root.registerAllEnumerators(enums);
  for (auto &[name, dev] : enums) {
    dev->enumerate([func] (const QString& name, const Device::DeviceSettings& s) {
        func(s.deviceSpecificSettings.value<CameraSettings>(), name);
    });
    delete dev;
  }
}

std::shared_ptr<CameraDeviceEnumerator> make_camera_enumerator()
{
  return std::make_shared<AVFCameraDeviceEnumerator>();
}
}

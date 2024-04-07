#include "CameraDevice.hpp"

#include "CameraSettings.hpp"

#include <Gfx/CameraDeviceEnumerator.hpp>

#import <AVFoundation/AVFoundation.h>
#include <libavutil/pixfmt.h>

#include <functional>
#include <iostream>
#include <memory>

namespace Gfx
{
static constexpr int avf_pixelformat_to_ffmpeg(uint32_t fourcc)
{
  switch(fourcc)
  {
    case kCMPixelFormat_32ARGB:
      return AV_PIX_FMT_ARGB;
    case kCMPixelFormat_32BGRA:
      return AV_PIX_FMT_BGRA;
    case kCMPixelFormat_24RGB:
      return AV_PIX_FMT_RGB24;
    case kCMPixelFormat_16BE555:
      return AV_PIX_FMT_RGB555BE;
    case kCMPixelFormat_16BE565:
      return AV_PIX_FMT_RGB565BE;
    case kCMPixelFormat_16LE555:
      return AV_PIX_FMT_RGB555LE;
    case kCMPixelFormat_16LE565:
      return AV_PIX_FMT_RGB565LE;
    case kCMPixelFormat_16LE5551:
      return 0;
    case 'yuv2':
    case 'YUV2':
    case 'uyvy':
    case 'UYVY':
    case kCMPixelFormat_422YpCbCr8:
      return AV_PIX_FMT_UYVY422;
    case kCMPixelFormat_422YpCbCr8_yuvs:
      return AV_PIX_FMT_YUYV422;
    case kCMPixelFormat_444YpCbCr8:
      return AV_PIX_FMT_YUV444P; // WARNING ! this is planar, macOS's is packed. check that it's ok and ffmpeg assumes the conversion
    case kCMPixelFormat_4444YpCbCrA8:
      return AV_PIX_FMT_YUVA444P;
    case kCMPixelFormat_422YpCbCr16:
      return AV_PIX_FMT_YUV422P16LE;
    case kCMPixelFormat_422YpCbCr10:
      return AV_PIX_FMT_YUV422P10LE;
    case kCMPixelFormat_444YpCbCr10:
      return AV_PIX_FMT_YUV444P10LE;
    case kCMPixelFormat_8IndexedGray_WhiteIsZero:
    default:
      return -1;
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
      std::string_view fcc((const char*)&fourcc, 4);
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
        set.pixelformat = avf_pixelformat_to_ffmpeg(fourcc);
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

std::shared_ptr<CameraDeviceEnumerator> make_camera_enumerator()
{
  return std::make_shared<AVFCameraDeviceEnumerator>();
}
} // namespace Gfx

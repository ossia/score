#import <AVFoundation/AVFoundation.h>

#include "CameraSettings.hpp"
#include  <QDebug>
#include <functional>
#include <libavutil/pixfmt.h>
namespace Gfx
{

static void iterateCameraFormats(AVCaptureDevice* device, std::function<void(CameraSettings, QString)> func)
{
  const char *name = [[device localizedName] UTF8String];
  for (id format in [device valueForKey:@"formats"]) {
    CMFormatDescriptionRef formatDescription{};
    CMVideoDimensions dimensions{};

    formatDescription = (CMFormatDescriptionRef) [format performSelector:@selector(formatDescription)];
    dimensions = CMVideoFormatDescriptionGetDimensions(formatDescription);

    for (id range in [format valueForKey:@"videoSupportedFrameRateRanges"]) {
      double fps{};

      [[range valueForKey:@"maxFrameRate"] getValue:&fps];

      const auto prettyName = QString("%1 (%2x%3@%4)").arg(name).arg(dimensions.width).arg(dimensions.height).arg(fps);

      func({.input = "avfoundation", .device = name, .size = { dimensions.width, dimensions.height}, .fps = fps }, prettyName);
    }
  }
}

void enumerateCameraDevices(std::function<void(CameraSettings, QString)> func)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  {
    NSArray *devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
    for (AVCaptureDevice *device in devices) {
      iterateCameraFormats(device, func);
    }
  }

  {
    NSArray *devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeMuxed];
    for (AVCaptureDevice *device in devices) {
      iterateCameraFormats(device, func);
    }
  }

  [pool release];
}
}

